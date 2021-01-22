import grpc
import grpc.aio
import asyncio
from typing import Callable, List, Union, Awaitable, Dict, Set, AsyncGenerator, cast, Literal

from src.vizier.vizierpb import vizier_pb2 as vpb
from src.vizier.vizierpb import vizier_pb2_grpc

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

    As a user, you can avoid directly initializing TableSub objects. Instead, you
    should create a Query object and `subscribe()` to a specific table or
    `Query.subscribe_all_tables()`. This avoids the complexity of creating a
    `table_gen` and ties in to the rest of the machinery to query Pixie data.

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

    def __init__(self, token: str, pixie_url: str, cluster_id: ClusterID):
        self.token = token
        self.url = pixie_url
        self.cluster_id = cluster_id

    def create_grpc_channel(self) -> grpc.aio.Channel:
        return grpc.aio.secure_channel(self.url, grpc.ssl_channel_credentials())


class Query:
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

        `subscribe()` must be called before `run_async()`, otherwise it will
        return an error.

        `subscribe()` may only be called once per table name. You can mimic the
        effect of multiple subscriptions by forwarding the rows to all or your
        consumers.

        If the table does not exist during the lifetime of the query, the
        subscription will throw an error.
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
        """ Adds a callback fn that will be invoked on every row of `table_name`. """
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
        """ Returns a generator that outputs table subscriptions as they arrive. """
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
        """ Executes the query asynchronously.
        Executes the query across all clusters. Returns any errors that might occur
        over the connection in a list. If no errors occur, will return a list of all Nones.
        """
        self._fail_on_multi_run()
        self._has_run = True
        await asyncio.gather(*[
            self._run_conn(conn, len(self._conns)) for conn in self._conns
        ], *[t() for t in self._tasks])

    def run(self) -> None:
        """ Executes the query synchronously """
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
                    raise build_pxl_exception(self._pxl, res.status)
                if res.HasField("meta_data"):
                    await self._process_metadata(conn.cluster_id, res.meta_data, num_conns)
                if res.HasField("data") and res.data.HasField("batch"):
                    await self._process_data(conn.cluster_id, res.data)
                elif res.HasField("data") and res.data.HasField("execution_stats"):
                    await self._set_exec_stats(conn.cluster_id, res.data.execution_stats)

        # TODO(philkuz) will cause an error if this is
        # sent in one conn before the other conn has sent its last table.
        self._close_table_q()
        await self._close_all_tables_for_cluster(conn.cluster_id)


class Client:
    def __init__(self, token: str, server_url: str = DEFAULT_PIXIE_URL):
        self._token = token
        self._server_url = server_url

    def query(self, conns: List[Conn], query_str: str) -> Query:
        return Query(conns, query_str)

    def _create_passthrough_conn(self, cluster_id: ClusterID) -> Conn:
        return Conn(self._token, self._server_url, cluster_id)

    def _create_direct_connection_conn(self, cluster_id: ClusterID) -> Conn:
        raise NotImplementedError("Direct connection not yet supported")

    def connect_to_cluster(self, cluster_id: ClusterID) -> Conn:
        # TODO(philkuz) add support for direct connections by making a Cloud API call here.
        return self._create_passthrough_conn(cluster_id)
