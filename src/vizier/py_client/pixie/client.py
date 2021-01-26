import grpc
import grpc.aio
from urllib.parse import urlparse
import asyncio
from typing import Callable, List, Union, Awaitable, Dict, Set, AsyncGenerator, cast, Literal

from src.vizier.vizierpb import vizier_pb2 as vpb
from src.vizier.vizierpb import vizier_pb2_grpc

from src.cloud.cloudapipb import cloudapi_pb2 as cpb
from src.cloud.cloudapipb import cloudapi_pb2_grpc

from src.common.uuid.proto import uuid_pb2 as uuidpb
from .data import (
    _TableStream,
    RowGenerator,
    Row,
    ClusterID,
    _Relation,
    _add_cluster_id_to_relation,
)

from .errors import (
    build_pxl_exception,
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
    should create a Query object and `Query.subscribe()` to a specific table or
    `Query.subscribe_all_tables()`. This avoids the complexity involved in creating this
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
                "'{}' not sent by query".format(self.table_name))

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
            cluster_info: cpb.ClusterInfo = None,
            channel_fn: Callable[[str], grpc.aio.Channel] = None,
            direct: bool = False,
    ):
        self.token = token
        self.url = pixie_url
        self.cluster_id = cluster_id

        self.cluster_info = cluster_info
        self._channel_fn = channel_fn

        # Whether the connection is direct connection or not.
        self._direct = direct

    def create_grpc_channel(self) -> grpc.aio.Channel:
        """ Creates a grpc channel from this connection. """
        if self._channel_fn:
            return self._channel_fn(self.url)
        creds = grpc.ssl_channel_credentials()
        if self._direct:
            tok = grpc.access_token_call_credentials(self.token)
            creds = grpc.composite_channel_credentials(creds, tok)
        return grpc.aio.secure_channel(self.url, creds)

    def name(self) -> str:
        """ Get the name of the cluster for this connection. """
        if self.cluster_info is None:
            return self.cluster_id
        return self.cluster_info.pretty_cluster_name


class Query:
    """
    Query encapsulates the connection logic to Pixie instances.

    If you want to get Pixie data, you will need to initialize `Query` with
    the clusters and PxL script, `add_callback()` for the desired table,
    and then `run()` the query.

    Note: you can only invoke `run()` once on a Query object. If you need
    to run a query multiple times, you must create a new Query object and
    setup the data processing again. We rely on iterators that must close
    when a query stops running and cannot allow multiple runs per object.
    """

    def __init__(self, conns: List[Conn], pxl: str):
        self._conns = conns
        if len(self._conns) == 0:
            raise ValueError("Query needs at least 1 connection to execute.")

        self._pxl = pxl

        # A mapping of the table ID to a table. The tables in the values aren't guaranteed to be
        # unique for different table_ids when a query uses multiple conns, as each conn produces
        # each named table, but each (connection, table_name) pair will have a corresponding
        # unique TableID.
        self._table_id_to_table_map: Dict[str, _TableStream] = {}
        # A mapping of the table name to a table. This will contain fewer entries than
        # `_table_id_to_table_map` because several ids will map to the same table.
        self._table_name_to_table_map: Dict[str, _TableStream] = {}
        self._tables_lock = asyncio.Lock()

        # The exec stats per query per id.
        self._exec_stats: Dict[ClusterID, vpb.QueryExecutionStats] = {}

        # Tracks whether the query has been run or not.
        self._has_run = False

        # Tables that have been subbed.
        self._subscribed_tables: Set[str] = set()

        # Flag whether to subscribe to all tables.
        self._subscribe_all_tables = False

        self._table_q_subscribers: List[asyncio.Queue[TableType]] = []
        self._tasks: List[Callable[[], Awaitable[None]]] = []

    def subscribe(self, table_name: str) -> TableSub:
        """ Returns an async generator that outputs rows for the table.

        Raises:
            ValueError: If called on a table that's already been passed as arg to
                `subscribe` or `add_callback`.
            ValueError: If called after `run()` or `run_async()` for a particular
                `Query`
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

        If you `add_callback` on a table not produced by the query, `run()`(or `run_async()`) will
        raise a ValueError when the underlying gRPC channel closes.


        The internals of `Query` use the python async api and the callback `fn`
        will be called concurrently while the Query is running. Note that callbacks
        themselves should not be async functions.

        Callbacks will block the rest of query execution so expensive and unending
        callbacks should not be used.

        Raises:
            ValueError: If called on a table that's already been passed as arg to
                `subscribe` or `add_callback`.
            ValueError: If called after `run()` or `run_async()` for a particular
                `Query`
        """
        table_sub = self.subscribe(table_name)

        async def callback_task() -> None:
            async for row in table_sub:
                fn(row)
        self._add_run_task(callback_task)

    def _is_table_subscribed(self, table_name: str) -> bool:
        return self._subscribe_all_tables or table_name in self._subscribed_tables

    def _add_table_q_subscriber(self) -> asyncio.Queue:
        """ Returns a queue that will receive new tables while the query runs. """
        q: asyncio.Queue[TableType] = asyncio.Queue()
        self._table_q_subscribers.append(q)
        return q

    def subscribe_all_tables(self) -> Callable[[], TableSubGenerator]:
        """
        Returns an async generator that outputs table subscriptions as they arrive.

        You can use this generator to call query PxL without knowing the tables
        that are output beforehand. If you do know the tables beforehand, you should
        `subscribe` instead as you won't keep data for tables that you don't use.

        This generator will only start iterating after `run_async()` has been
        called. For the best performance, you will want to call the consumer of
        the object returned by `subscribe_all_tables` concurrently with `run_async()`
        """
        self._fail_on_multi_run()
        self._subscribe_all_tables = True

        # You can only call create a table generator before Query.run_async(), therefore
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
        """ Add a table to notify subscribers when the table is first initiated in a query run. """
        for q in self._table_q_subscribers:
            q.put_nowait(table)

    async def _process_metadata(self,
                                cluster_id: ClusterID,
                                metadata: vpb.QueryMetadata,
                                num_conns: int) -> None:

        relation = _Relation(_add_cluster_id_to_relation(metadata.relation))

        async with self._tables_lock:
            # Don't create a new table if we've already seen one with the same name.
            if metadata.name in self._table_name_to_table_map:
                table = self._table_name_to_table_map[metadata.name]
                if relation != table.relation:
                    raise ValueError("Table relation '{}' does not match "
                                     "existing relation '{}' for '{}'".format(
                                         relation, table.relation, metadata.name))
                self._table_id_to_table_map[metadata.id] = table
                table.add_cluster_table_id(metadata.id, cluster_id)
                return

            table = _TableStream(
                metadata.name,
                relation,
                num_conns,
                subscribed=self._is_table_subscribed(metadata.name)
            )
            table.add_cluster_table_id(metadata.id, cluster_id)
            self._table_id_to_table_map[metadata.id] = table
            self._table_name_to_table_map[metadata.name] = table

        self._add_table_to_q(table)

    async def _process_data(self, cluster_id: ClusterID, data: vpb.QueryData) -> None:
        table_id = data.batch.table_id
        async with self._tables_lock:
            assert table_id in self._table_id_to_table_map, "id is missing " + table_id
            self._table_id_to_table_map[table_id].add_row_batch(
                cluster_id, data.batch)

    async def _set_exec_stats(self,
                              cluster_id: ClusterID,
                              exec_stats: vpb.QueryExecutionStats) -> None:
        self._exec_stats[cluster_id] = exec_stats

    def _fail_on_multi_run(self) -> None:
        """
        Raise an error if run() has been called on this object.

        Query objects are not setup to be run more than once due to the design of _TableStreams.
        _TableStreams belonging to a query are closed when the query finishes executing. If users
        could rerun a query users might expect _TableStreams to continue returning data from
        the new query run.

        Instead, create a new query object and subscribe() to the original _TableStreams
        if you wish to rerun queries.
        """

        # Function raises an error if the query has ran before.
        if not self._has_run:
            return

        raise ValueError("Query already ran. Cannot perform action.")

    async def run_async(self) -> None:
        """ Runs the query asynchronously using asyncio.

        Same as `run()` except you can directly control whether other tasks
        should be run concurrently while the query is running.

        Raises:
            ValueError: If any callbacks are on tables that a `Query` never receives.
            ValueError: If called after `run()` or `run_async()` for a particular
                `Query`.
        """
        self._fail_on_multi_run()
        self._has_run = True
        await asyncio.gather(*[
            self._run_conn(conn, len(self._conns)) for conn in self._conns
        ], *[t() for t in self._tasks])

    def run(self) -> None:
        """ Executes the query synchronously.

        Executes the query across all clusters. If any errors occur over the lifetime of
        any connection, this will raise an error.

        Raises:
            ValueError: If any callbacks are on tables that a `Query` never receives.
            ValueError: If called after `run()` or `run_async()` for a particular
                `Query`.
        """
        loop = asyncio.get_event_loop()
        return loop.run_until_complete(self.run_async())

    def _close_table_q(self) -> None:
        self._add_table_to_q(EOF)

    async def _close_all_tables_for_cluster(self, cluster_id: ClusterID) -> None:
        async with self._tables_lock:
            for name, table in self._table_name_to_table_map.items():
                table.close(cluster_id)

    async def _run_conn(self, conn: Conn, num_conns: int) -> None:
        """ Executes the query on a single connection. """
        async with conn.create_grpc_channel() as channel:
            stub = vizier_pb2_grpc.VizierServiceStub(channel)

            req = vpb.ExecuteScriptRequest()
            req.cluster_id = conn.cluster_id
            req.query_str = self._pxl

            async for res in stub.ExecuteScript(req, metadata=[
                ("pixie-api-key", conn.token),
                ("pixie-api-client", "python"),
            ]):
                if res.status.code != 0:
                    self._add_table_to_q(QUERY_ERROR)
                    await self._close_all_tables_for_cluster(conn.cluster_id)
                    raise build_pxl_exception(
                        self._pxl, res.status, conn.name())
                if res.HasField("meta_data"):
                    await self._process_metadata(conn.cluster_id, res.meta_data, num_conns)
                if res.HasField("data") and res.data.HasField("batch"):
                    await self._process_data(conn.cluster_id, res.data)
                elif res.HasField("data") and res.data.HasField("execution_stats"):
                    await self._set_exec_stats(conn.cluster_id, res.data.execution_stats)

        # TODO(philkuz) will cause an error if this is
        # sent in one conn before the other conn has sent its last table metadata.
        # Bug is unlikely to happen, but should still be covered.
        self._close_table_q()
        await self._close_all_tables_for_cluster(conn.cluster_id)


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
        return self.info.pretty_cluster_name


class Client:
    """
    Client is the main entry point to the Pixie API.

    To setup the client, you need to generate an API token
    and pass it in as the first argument.
    See: https://docs.pixielabs.ai/using-pixie/interfaces/using-api/
    for more info.
    """

    def __init__(
        self,
        token: str,
        server_url: str = DEFAULT_PIXIE_URL,
        channel_fn: Callable[[str], grpc.Channel] = None,
        conn_channel_fn: Callable[[str], grpc.aio.Channel] = None,
    ):
        self._token = token
        self._server_url = server_url
        self._channel_fn = channel_fn
        self._conn_channel_fn = conn_channel_fn

    def query(self, conns: List[Conn], query_str: str) -> Query:
        """ Create a new Query object from this client. """
        return Query(conns, query_str)

    def _get_cloud_channel(self) -> grpc.Channel:
        if self._channel_fn:
            return self._channel_fn(self._server_url)
        return grpc.secure_channel(self._server_url, grpc.ssl_channel_credentials())

    def _all_clusters(self) -> cpb.ClusterInfo:
        with self._get_cloud_channel() as channel:
            stub = cloudapi_pb2_grpc.VizierClusterInfoStub(channel)
            request = cpb.GetClusterInfoRequest()
            response: cpb.GetClusterInfoResponse = stub.GetClusterInfo(request, metadata=[
                ("pixie-api-key", self._token),
                ("pixie-api-client", "python")
            ])
            return response.clusters

    def list_healthy_clusters(self) -> List[Cluster]:
        """ Lists all of the healthy clusters within the Pixie org. """
        healthy_clusters: List[Cluster] = []
        for c in self._all_clusters():
            if c.status != cpb.CS_HEALTHY:
                continue
            healthy_clusters.append(
                Cluster(
                    cluster_id=c.id.data.decode('utf-8'),
                    cluster_info=c,
                )
            )

        return healthy_clusters

    def _get_cluster_info(self, cluster_id: ClusterID) -> cpb.ClusterInfo:
        with self._get_cloud_channel() as channel:
            stub = cloudapi_pb2_grpc.VizierClusterInfoStub(channel)
            request = cpb.GetClusterInfoRequest(
                id=uuidpb.UUID(
                    data=cluster_id.encode('utf-8'))
            )
            response = stub.GetClusterInfo(request, metadata=[
                ("pixie-api-key", self._token),
                ("pixie-api-client", "python")
            ])
            return response.clusters[0]

    def _get_cluster_connection_info(
            self,
            cluster_id: ClusterID
    ) -> cpb.GetClusterConnectionInfoResponse:
        with self._get_cloud_channel() as channel:
            stub = cloudapi_pb2_grpc.VizierClusterInfoStub(channel)
            request = cpb.GetClusterConnectionInfoRequest(
                id=uuidpb.UUID(
                    data=cluster_id.encode('utf-8'))
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
            cluster_info,
            self._conn_channel_fn,
        )

    def _create_direct_connection(
        self,
        cluster_id: ClusterID,
        cluster_info: cpb.ClusterInfo,
    ) -> Conn:
        resp = self._get_cluster_connection_info(cluster_id)
        token, cluster_url = resp.token, urlparse(resp.ipAddress).netloc
        return Conn(
            token,
            cluster_url,
            cluster_id,
            cluster_info,
            self._conn_channel_fn,
            direct=True,
        )

    def connect_to_cluster(self,
                           cluster: Union[ClusterID, Cluster]
                           ) -> Conn:
        """ Connect to a cluster.

        Returns a connection object that must be passed as an argument to `query()`
        with the query you wish to send over.
        """
        cluster_info: cpb.ClusterInfo = None
        if isinstance(cluster, ClusterID):
            cluster_id = cast(ClusterID, cluster)
        elif isinstance(cluster, Cluster):
            cluster_id = cluster.id
        else:
            raise ValueError("Unexpected type for `cluster`: ", type(cluster))

        cluster_info = self._get_cluster_info(cluster_id)
        if cluster_info.config.passthrough_enabled:
            return self._create_passthrough_conn(cluster_id, cluster_info)
        else:
            return self._create_direct_connection(cluster_id, cluster_info)
