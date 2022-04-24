# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import asyncio
import grpc
import grpc.aio
import warnings
from typing import AsyncGenerator, Awaitable, Callable, cast, \
    Dict, Generator, List, Literal, Union, Set

from src.api.proto.vizierpb import vizierapi_pb2 as vpb
from src.api.proto.vizierpb import vizierapi_pb2_grpc

from src.api.proto.cloudpb import cloudapi_pb2 as cpb
from src.api.proto.cloudpb import cloudapi_pb2_grpc

from .data import (
    _TableStream,
    RowGenerator,
    Row,
    ClusterID,
    _Relation,
)

from .errors import (
    build_pxl_exception,
)

from .utils import (
    CryptoOptions,
    decode_row_batch,
    uuid_pb_from_string,
    uuid_pb_to_string,
)


DEFAULT_PIXIE_URL = "work.withpixie.ai"

EOF = None
QUERY_ERROR: Literal["ERROR"] = "ERROR"
TableOrError = Union[_TableStream, Literal["ERROR"]]
_TableStreamGenerator = AsyncGenerator[TableOrError, None]


class TableSub:
    """
    TableSub is an async generator that yields rows for table.

    You should avoid directly initializing TableSub objects. Instead, you
    should create a ScriptExecutor object and `ScriptExecutor.subscribe()` to a specific table or
    `ScriptExecutor.subscribe_all_tables()`. This avoids the complexity involved in creating this
    object.

    For more advanced users: the TableSub object is a promise that a table with the specified name
    will be yielded by the `table_gen`. If the table does not get yielded, the async generator will
    throw an error when the `table_gen` exits.
    """

    def __init__(self,
                 name: str,
                 table_gen: _TableStreamGenerator):
        self.table_name = name
        self._table_gen = table_gen

    async def __aiter__(self) -> RowGenerator:
        table_stream = None
        async for t in self._table_gen:
            if t == QUERY_ERROR:
                return
            table = cast(_TableStream, t)
            if table.name == self.table_name:
                table_stream = table
                break

        if table_stream is None:
            raise ValueError(
                "Table '{}' not received".format(self.table_name))

        async for row in table_stream:
            yield row


TableType = Union[TableOrError, None]
TableSubGenerator = AsyncGenerator[TableSub, None]


class Conn:
    """
    The logical representation of a connection.

    Holds the authorization information and handles the creation
    of an authorized gRPC channel.
    """

    def __init__(
            self,
            token: str,
            pixie_url: str,
            cluster_id: ClusterID,
            use_encryption: bool = True,
            cluster_info: cpb.ClusterInfo = None,
            channel_fn: Callable[[str], grpc.aio.Channel] = None,
    ):
        self.token = token
        self.url = pixie_url
        self.cluster_id = cluster_id

        self.cluster_info = cluster_info
        self._channel_fn = channel_fn

        self._channel_cache: grpc.aio.Channel = None

        self._use_encryption = use_encryption

    def prepare_script(self, script_str: str) -> 'ScriptExecutor':
        """ Create a new ScriptExecutor for the script to run on this connection. """
        return ScriptExecutor(self, script_str, use_encryption=self._use_encryption)

    def _get_grpc_channel(self) -> grpc.aio.Channel:
        """
        Gets the grpc_channel for this connection.
        """
        # Always create GRPC channel. Asyncio channels hold onto loop state and it's unsafe to
        # cache them, incase the loop changes between connection runs.
        return self._create_grpc_channel()

    def _create_grpc_channel(self) -> grpc.aio.Channel:
        """ Creates a grpc channel for this connection. """
        if self._channel_fn:
            return self._channel_fn(self.url)
        creds = grpc.ssl_channel_credentials()
        return grpc.aio.secure_channel(self.url, creds)

    def name(self) -> str:
        """ Get the name of the cluster for this connection. """
        if self.cluster_info is None:
            return self.cluster_id
        return self.cluster_info.cluster_name


class ScriptExecutor:
    """
    ScriptExecutor encapsulates the connection logic to Pixie instances.

    If you want to get Pixie data, you will need to initialize `ScriptExecutor` with
    the clusters and PxL script then call `results()` for the desired table name and
    iterate the results.

    Note: you can only invoke `results()`, `run()`,and `run_async()` once on a
    `ScriptExecutor` object. If you need to exeucte a script multiple times,
    you must create a new `ScriptExecutor` object and setup any data processing
    again. We rely on iterators that must close when a script stops running
    and cannot allow multiple runs per object.
    """

    def __init__(self, conn: Conn, pxl: str, use_encryption: bool):
        self._conn = conn
        self._pxl = pxl

        # A mapping of the table ID to a table. We use this to map incoming data which only
        # has the table ID to the proper table.
        self._table_id_to_table_map: Dict[str, _TableStream] = {}
        # A mapping of the table name to a table.
        self._table_name_to_table_map: Dict[str, _TableStream] = {}
        self._tables_lock = asyncio.Lock()

        # The execution stats for the script.
        self._exec_stats: vpb.QueryExecutionStats = None

        # Tracks whether the script has been run or not.
        self._has_run = False

        # Tables that have been subbed.
        self._subscribed_tables: Set[str] = set()

        # Flag whether to subscribe to all tables.
        self._subscribe_all_tables = False

        # Whether to encrypt the execution or not.
        self._use_encryption: bool = use_encryption
        self._crypto = None

        self._table_q_subscribers: List[asyncio.Queue[TableType]] = []
        self._tasks: List[Callable[[], Awaitable[None]]] = []

    def subscribe(self, table_name: str) -> TableSub:
        """ Returns an async generator that outputs rows for the table.

        Raises:
            ValueError: If called on a table that's already been passed as arg to
                `subscribe` or `add_callback`.
            ValueError: If called after `run()` or `run_async()` for a particular
                `ScriptExecutor`
        """
        self._fail_on_multi_run()
        if self._is_table_subscribed(table_name):
            raise ValueError(
                ("Already subscribed to '{}'. Wrap the first subscription "
                 "to enable multiple subscribers.").format(table_name))

        sub = TableSub(table_name, self._tables())
        self._subscribed_tables.add(table_name)
        return sub

    def _add_run_task(self, task: Callable[[], Awaitable[None]]) -> None:
        """ Adds a task concurrently with async """
        self._tasks.append(task)

    def add_callback(self, table_name: str, fn: Callable[[Row], None]) -> None:
        """
        Adds a callback fn that will be invoked on every row of `table_name` as
        they arrive.

        Callbacks are not invoked until you call `run()` (or `run_async()`) on
        the object.

        If you `add_callback` on a table not produced by the script, `run()`(or `run_async()`) will
        raise a ValueError when the underlying gRPC channel closes.

        The internals of `ScriptExecutor` use the python async api and the callback `fn`
        will be called concurrently while the ScriptExecutor is running. Note that callbacks
        themselves should not be async functions.

        Callbacks will block the rest of script execution so expensive and unending
        callbacks should not be used.

        Raises:
            ValueError: If called on a table that's already been passed as arg to
                `subscribe` or `add_callback`.
            ValueError: If called after `run()` or `run_async()` for a particular
                `ScriptExecutor`
        """
        table_sub = self.subscribe(table_name)

        async def callback_task() -> None:
            async for row in table_sub:
                fn(row)
        self._add_run_task(callback_task)

    def _is_table_subscribed(self, table_name: str) -> bool:
        return self._subscribe_all_tables or table_name in self._subscribed_tables

    def _add_table_q_subscriber(self) -> asyncio.Queue:
        """ Returns a queue that will receive new tables while the script runs. """
        q: asyncio.Queue[TableType] = asyncio.Queue()
        self._table_q_subscribers.append(q)
        return q

    def subscribe_all_tables(self) -> Callable[[], TableSubGenerator]:
        """
        Returns an async generator that outputs table subscriptions as they arrive.

        You can use this generator to call PxL scripts without knowing the tables
        that are output beforehand. If you do know the tables beforehand, you should
        `subscribe`, `add_callback` or even `results` instead to prevent your api from
        keeping data for tables that you don't use.

        This generator will only start iterating after `run_async()` has been
        called. For the best performance, you will want to call the consumer of
        the object returned by `subscribe_all_tables` concurrently with `run_async()`
        """
        self._fail_on_multi_run()
        self._subscribe_all_tables = True

        # You can only call create a table generator before ScriptExecutor.run_async(), therefore
        # we declare it outside of the TableSubGenerator.
        tables = self._tables()

        async def generate_table(table: _TableStream) -> _TableStreamGenerator:
            yield table

        async def internal() -> TableSubGenerator:
            async for t in tables:
                if t == QUERY_ERROR:
                    return
                table = cast(_TableStream, t)
                sub = TableSub(table.name, generate_table(table))
                yield sub

        return internal

    def _tables(self) -> _TableStreamGenerator:
        """
        Returns a generator for all table streams as they arrive.

        External users should use `subscribe_all_tables()` instead of this generator.
        This generator is an internal function used by public methods of this API to
        received _TableStream objects which will not have data if a user has not subscribed
        to the specified table name.
        """
        self._fail_on_multi_run()
        q = self._add_table_q_subscriber()

        async def internal() -> _TableStreamGenerator:
            while True:
                new_table = await q.get()
                if new_table == EOF:
                    q.task_done()
                    return
                q.task_done()
                yield cast(_TableStream, new_table)
        return internal()

    def _add_table_to_q(self, table: Union[TableOrError, None]) -> None:
        """ Add a table to notify subscribers when the table is first initiated in a script run. """
        for q in self._table_q_subscribers:
            q.put_nowait(table)

    async def _process_metadata(self,
                                metadata: vpb.QueryMetadata) -> None:
        relation = _Relation(metadata.relation)

        async with self._tables_lock:
            table = _TableStream(
                metadata.name,
                relation,
                subscribed=self._is_table_subscribed(metadata.name)
            )
            self._table_id_to_table_map[metadata.id] = table
            self._table_name_to_table_map[metadata.name] = table

        self._add_table_to_q(table)

    async def _process_encrypted_batch(self, encrypted_batch: str) -> None:
        batch = decode_row_batch(self._crypto, encrypted_batch)
        await self._process_data_batch(batch)

    async def _process_data_batch(self, batch: vpb.RowBatchData) -> None:
        table_id = batch.table_id
        async with self._tables_lock:
            assert table_id in self._table_id_to_table_map, "id is missing " + table_id
            self._table_id_to_table_map[table_id].add_row_batch(batch)

    async def _set_exec_stats(self,
                              exec_stats: vpb.QueryExecutionStats) -> None:
        self._exec_stats = exec_stats

    def _fail_on_multi_run(self) -> None:
        """
        Raise an error if `run_async()` has been called on this object.

        `ScriptExecutor` objects are not setup to be run more than once
        due to the design of `TableSub`. `TableSub`s belonging to a
        `ScriptExecutor` are closed when the `ScriptExecutor` finishes executing. Otherwise
        you might expect subsequent `ScriptExecutor` runs to continue yielding data
        which does not work with the current architecture.

        If you need such functionality, create a new `ScriptExecutor` and
        set up your data processing again.
        """

        # Function raises an error if the script has ran before.
        if not self._has_run:
            return

        raise ValueError("Script already executed. Cannot perform action.")

    async def run_async(self) -> None:
        """ Runs the script asynchronously using asyncio.

        Same as `run()` except you can directly control whether other tasks
        should be run concurrently while the script  is running.

        Raises:
            ValueError: If any callbacks are on tables that a `ScriptExecutor` never receives.
            ValueError: If called after `run()` or `run_async()` for a particular
                `ScriptExecutor`.
        """
        self._fail_on_multi_run()
        self._has_run = True
        # Runs the script itself + all of the "tasks" (table processors) asynchronously.
        await asyncio.gather(self._run_conn(self._conn), *[t() for t in self._tasks])

    def run(self) -> None:
        """ Executes the script synchronously.

        Calls `run_async()` but hides the asyncio details from users.
        If any errors occur over the lifetime of any connection, this will raise an error.

        Raises:
            ValueError: If any callbacks are on tables that a `ScriptExecutor` never receives.
            ValueError: If called after `run()` or `run_async()` for a particular
                `ScriptExecutor`.
        """
        loop = asyncio.get_event_loop()
        loop.run_until_complete(self.run_async())

    def _close_table_q(self) -> None:
        self._add_table_to_q(EOF)

    async def _close_all_tables(self) -> None:
        async with self._tables_lock:
            for name, table in self._table_name_to_table_map.items():
                table.close()

    def results(self, table_name: str) -> Generator[Row, None, None]:
        """ Runs script and return results for the table.
        Examples:
            for row in script.results("http_table"):
                print(row)
        Raises:
            ValueError: If `table_name` is never sent during lifetime of script.
            ValueError: If called after `run()` or `run_async()` for a particular
                `ScriptExecutor`.
        """
        rows = []

        def _cb(row: Row) -> None:
            rows.append(row)

        self.add_callback(table_name, _cb)
        self.run()
        # TODO(philkuz,PP-2424) update the run call to be multi-threaded
        # to avoid accumulating memory for rows.
        for r in rows:
            yield r

    async def _run_conn(self, conn: Conn) -> None:
        """ Executes the script on a single connection. """
        channel = conn._get_grpc_channel()
        stub = vizierapi_pb2_grpc.VizierServiceStub(channel)

        req = vpb.ExecuteScriptRequest()
        req.cluster_id = conn.cluster_id
        req.query_str = self._pxl

        if self._use_encryption:
            self._crypto = CryptoOptions()
            req.encryption_options.CopyFrom(self._crypto.encrypt_options())

        async for res in stub.ExecuteScript(req, metadata=[
            ("pixie-api-key", conn.token),
            ("pixie-api-client", "python"),
        ]):
            if res.status.code != 0:
                self._add_table_to_q(QUERY_ERROR)
                await self._close_all_tables()
                raise build_pxl_exception(
                    self._pxl, res.status, conn.name())
            if res.HasField("meta_data"):
                await self._process_metadata(res.meta_data)
            if res.HasField("data") and len(res.data.encrypted_batch) > 0:
                if not self._use_encryption:
                    raise ValueError("Received encrypted data on unencrypted request")
                if self._crypto is None:
                    raise ValueError("Error while trying to decrypt batch, cryptography information not saved by API")
                await self._process_encrypted_batch(res.data.encrypted_batch)
            if res.HasField("data") and res.data.HasField("batch"):
                if self._use_encryption:
                    warnings.warn("Received unencrypted data on encrypted request")
                await self._process_data_batch(res.data.batch)
            elif res.HasField("data") and res.data.HasField("execution_stats"):
                await self._set_exec_stats(res.data.execution_stats)

        self._close_table_q()
        await self._close_all_tables()


class Cluster:
    """ Cluster contains information users need about a specific cluster.

    Mainly a convenience wrapper around the protobuf message so you
    can access the name in a simple format.
    """

    def __init__(self, cluster_id: str, cluster_info: cpb.ClusterInfo):
        self.id = cluster_id
        self.info = cluster_info

    def name(self) -> str:
        """ Returns the name if that info exists, otherwise returns the id. """
        if self.info is None:
            return self.id
        return self.info.cluster_name

    def passthrough(self) -> bool:
        return self.info.config.passthrough_enabled


class Client:
    """
    Client is the main entry point to the Pixie API.

    To setup the client, you need to generate an API token
    and pass it in as the first argument.
    See: https://docs.px.dev/using-pixie/api-quick-start/
    for more info.
    """

    def __init__(
        self,
        token: str,
        server_url: str = DEFAULT_PIXIE_URL,
        use_encryption: bool = False,
        channel_fn: Callable[[str], grpc.Channel] = None,
        conn_channel_fn: Callable[[str], grpc.aio.Channel] = None,
    ):
        self._token = token
        self._server_url = server_url
        self._channel_fn = channel_fn
        self._conn_channel_fn = conn_channel_fn
        self._cloud_channel_cache: grpc.Channel = None
        self._use_encryption = use_encryption

    def _create_cloud_channel(self) -> grpc.Channel:
        if self._channel_fn:
            return self._channel_fn(self._server_url)
        return grpc.secure_channel(self._server_url, grpc.ssl_channel_credentials())

    def _get_cloud_channel(self) -> grpc.Channel:
        if self._cloud_channel_cache is None:
            self._cloud_channel_cache = self._create_cloud_channel()

        return self._cloud_channel_cache

    def _get_cluster(self, request: cpb.GetClusterInfoRequest) -> List[cpb.ClusterInfo]:
        stub = cloudapi_pb2_grpc.VizierClusterInfoStub(self._get_cloud_channel())
        response: cpb.GetClusterInfoResponse = stub.GetClusterInfo(request, metadata=[
            ("pixie-api-key", self._token),
            ("pixie-api-client", "python")
        ])
        return response.clusters

    def list_healthy_clusters(self) -> List[Cluster]:
        """ Lists all of the healthy clusters that you can access.  """
        healthy_clusters: List[Cluster] = []
        for c in self._get_cluster(cpb.GetClusterConnectionInfoRequest()):
            if c.status != cpb.CS_HEALTHY:
                continue
            healthy_clusters.append(
                Cluster(
                    cluster_id=uuid_pb_to_string(c.id),
                    cluster_info=c,
                )
            )

        return healthy_clusters

    def _get_cluster_info(self, cluster_id: ClusterID) -> cpb.ClusterInfo:
        request = cpb.GetClusterInfoRequest(
            id=uuid_pb_from_string(cluster_id)
        )
        return self._get_cluster(request)[0]

    def _get_cluster_connection_info(
            self,
            cluster_id: ClusterID
    ) -> cpb.GetClusterConnectionInfoResponse:
        channel = self._get_cloud_channel()
        stub = cloudapi_pb2_grpc.VizierClusterInfoStub(channel)
        request = cpb.GetClusterConnectionInfoRequest(
            id=uuid_pb_from_string(cluster_id)
        )
        response = stub.GetClusterConnectionInfo(request, metadata=[
            ("pixie-api-key", self._token),
            ("pixie-api-client", "python")
        ])
        return response

    def _create_passthrough_conn(
        self,
        cluster_id: ClusterID,
        cluster_info: cpb.ClusterInfo,
    ) -> Conn:
        return Conn(
            self._token,
            self._server_url,
            cluster_id,
            self._use_encryption,
            cluster_info=cluster_info,
            channel_fn=self._conn_channel_fn,
        )

    def connect_to_cluster(self,
                           cluster: Union[ClusterID, Cluster]
                           ) -> Conn:
        """ Connect to a cluster.

        Returns a connection object that you can use to create `ScriptExecutor`s.
        You may pass in a `ClusterID` string or a `Cluster` object that comes
        from `list_all_healthy_clusters()`.
        """
        cluster_info: cpb.ClusterInfo = None
        if isinstance(cluster, ClusterID):
            cluster_id = cast(ClusterID, cluster)
        elif isinstance(cluster, Cluster):
            cluster_id = cluster.id
        else:
            raise ValueError("Unexpected type for 'cluster': ", type(cluster))

        cluster_info = self._get_cluster_info(cluster_id)
        return self._create_passthrough_conn(cluster_id, cluster_info)
