import unittest
import grpc
import asyncio
from concurrent import futures
from typing import List, Any, Coroutine, Dict


import pixie
from pixie import vizier_pb2_grpc, vpb, test_utils as utils

ACCESS_TOKEN = "12345678-0000-0000-0000-987654321012"

query_str = """
import px
px.display(px.DataFrame('http_events')[
           ['http_resp_body','http_resp_status']].head(10), 'http')
px.display(px.DataFrame('process_stats')[
           ['upid','cpu_ktime_ns', 'rss_bytes']].head(10), 'stats')
"""


async def run_query_and_tasks(
    query: pixie.Query,
    processors: List[Coroutine[Any, Any, Any]]
) -> None:
    """
    Runs data processors in parallel with a query, returns the result of the coroutine.
    """
    tasks = [asyncio.create_task(p) for p in processors]
    try:
        await query.run_async()
    except Exception as e:
        for t in tasks:
            t.cancel()
        raise e

    await asyncio.gather(*tasks)


class VizierServiceFake:
    def __init__(self) -> None:
        self.cluster_id_to_fake_data: Dict[str,
                                           List[vpb.ExecuteScriptResponse]] = {}
        self.cluster_id_to_error: Dict[str, Exception] = {}

    def add_fake_data(self, cluster_id: str, data: List[vpb.ExecuteScriptResponse]) -> None:
        if cluster_id not in self.cluster_id_to_fake_data:
            self.cluster_id_to_fake_data[cluster_id] = []
        self.cluster_id_to_fake_data[cluster_id].extend(data)

    def trigger_error(self, cluster_id: str, exc: Exception) -> None:
        """ Adds an error that triggers after the data is yielded. """
        self.cluster_id_to_error[cluster_id] = exc

    def ExecuteScript(self, request: vpb.ExecuteScriptRequest, context: Any) -> Any:
        cluster_id = request.cluster_id
        assert cluster_id in self.cluster_id_to_fake_data, "need data for cluster_id"
        data = self.cluster_id_to_fake_data[cluster_id]
        for d in data:
            yield d
        # Trigger an error for the cluster ID if the user added one.
        if cluster_id in self.cluster_id_to_error:
            raise self.cluster_id_to_error[cluster_id]

    def HealthCheck(self, request: Any, context: Any) -> Any:
        yield vpb.Status(code=1, message="fail")


class FakeConn(pixie.Conn):
    """
    FakeConn overrides Conn to use an insecure_channel instead
    of a secure one.
    """

    def __init__(self, token: str, pixie_url: str, cluster_id: pixie.ClusterID):
        self.token = token
        self.url = pixie_url
        self.cluster_id = cluster_id

    def create_grpc_channel(self) -> grpc.aio.Channel:
        return grpc.aio.insecure_channel(self.url)


class TestClient(unittest.TestCase):
    server_class = VizierServiceFake
    port = 50051

    def setUp(self) -> None:
        # Create a fake server for the VizierService
        self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=1))
        self.service = self.server_class()

        vizier_pb2_grpc.add_VizierServiceServicer_to_server(
            self.service, self.server)
        self.server.add_insecure_port(f"[::]:{self.port}")
        self.server.start()

        self.http_table_factory = utils.FakeTableFactory("http", vpb.Relation(columns=[
            utils.string_col("http_resp_body"),
            utils.int64_col("http_resp_status"),
        ]))

        self.stats_table_factory = utils.FakeTableFactory("stats", vpb.Relation(columns=[
            utils.uint128_col("upid"),
            utils.int64_col("cpu_ktime_ns"),
            utils.int64_col("rss_bytes"),
        ]))

    def url(self) -> str:
        return f"localhost:{self.port}"

    def tearDown(self) -> None:
        self.server.stop(None)

    def test_one_conn_one_table(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to a single fake cluster.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Create table for cluster_uuid1.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([["foo"], [200]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Create the query object.
        query = px_client.query(conns, query_str)
        # Subscribe to the http table.
        http_tb = query.subscribe("http")

        # Define an async function that processes the TableSub
        # while the API query can run concurrently.
        async def process_table(table_sub: pixie.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
                self.assertEqual(row["http_resp_body"], "foo")
                self.assertEqual(row["http_resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Run the query and process_table concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_query_and_tasks(query, [process_table(http_tb)]))

    def test_multiple_rows_and_rowbatches(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to one fake cluster.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Create table for the first cluster.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        rb_data: List[List[Any]] = [
            ["foo", "bar", "baz", "bat"], [200, 500, 301, 404]]

        # Here we split the above data into two rowbatches.
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Row batch 1 has data 1-3,
            http_table1.row_batch_response([rb_data[0][:3], rb_data[1][:3]]),
            # Row batch 2 has data 4
            http_table1.row_batch_response([rb_data[0][3:], rb_data[1][3:]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Create the query object.
        query = px_client.query(conns, query_str)
        # Subscribe to the http table.
        http_tb = query.subscribe("http")

        # Verify that the rows returned by the table_sub match the
        # order and values of the input test data.
        async def process_table(table_sub: pixie.TableSub) -> None:
            row_i = 0
            # table_sub hides the batched rows and delivers them in
            # the same order as the batches sent.
            async for row in table_sub:
                self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
                self.assertEqual(row["http_resp_body"], rb_data[0][row_i])
                self.assertEqual(row["http_resp_status"], rb_data[1][row_i])
                row_i += 1

            self.assertEqual(row_i, 4)

        # Run the query and process_table concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_query_and_tasks(query, [process_table(http_tb)]))

    def test_two_conns_one_table(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to two fake clusters.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1),
                 FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid2)]

        # Create table for the first cluster.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([["foo"], [200]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Create table for the second cluster. The tables are named the
        # same and should have the same relation.
        http_table2 = self.http_table_factory.create_table(utils.table_id2)
        self.service.add_fake_data(utils.cluster_uuid2, [
            # Initialize the table on the stream with the metadata. The query
            # handler should recognize this table as the same as the one
            # sent by http_table1.
            http_table2.metadata_response(),
            # Send over a single-row batch.
            http_table2.row_batch_response([["bar"], [400]]),
            # Send an end-of-stream for the table.
            http_table2.end(),
        ])

        # Create the query obejct.
        query = px_client.query(conns, query_str)
        # Subscribe to the http table. We need only one subscription
        # because the Query merges each connection's table representation.
        http_tb = query.subscribe("http")

        # Define an async function that processes the TableSub
        # while the API query can run concurrently.
        async def process_table(table_sub: pixie.TableSub) -> None:
            num_rows = 0
            seen_clusters = set()

            # Rows from both clusters should be merged in the table_sub.
            async for row in table_sub:
                if row["cluster_id"] == utils.cluster_uuid1:
                    self.assertEqual(row["http_resp_body"], "foo")
                    self.assertEqual(row["http_resp_status"], 200)
                elif row["cluster_id"] == utils.cluster_uuid2:
                    self.assertEqual(row["http_resp_body"], "bar")
                    self.assertEqual(row["http_resp_status"], 400)
                num_rows += 1
                seen_clusters.add(row["cluster_id"])

            # Verify that we only received two rows.
            self.assertEqual(num_rows, 2)
            # Verify that both clusters sent data (one row per cluster).
            self.assertEqual(seen_clusters, {
                utils.cluster_uuid1, utils.cluster_uuid2})

        # Run the query and process_table concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_query_and_tasks(query, [process_table(http_tb)]))

    def test_mismatching_table_across_conns(self) -> None:
        # Tests to make sure we error out early if conns send over mismatching tables.
        # We error out to prevent the propagation of errors down the line.
        # This type of error usually means the cluster versions are vastly different.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1),
                 FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid2)]

        # Create table for the first cluster.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            http_table1.end(),
        ])

        # Create table for the second cluster, but modify the relation to have an extra column.
        # This extra column should trigger an error while query is running.
        http_table2 = self.http_table_factory.create_table(utils.table_id2)
        extra_col_relation = vpb.Relation(columns=http_table2.relation.columns)
        extra_col_relation.columns.append(vpb.Relation.ColumnInfo(
            column_name="extra_col", column_type=vpb.STRING))
        http_table2.relation = extra_col_relation

        self.service.add_fake_data(utils.cluster_uuid2, [
            # Initialize this table, send over the problematic relation above.
            http_table2.metadata_response(),
            # Send data over for the table.
            http_table2.row_batch_response([["bar"], [400], ["extra"]]),
            # Send an end-of-stream for the table.
            http_table2.end(),
        ])

        query = px_client.query(conns, query_str)
        # Run the query and expect an exception to be Raised.
        with self.assertRaisesRegex(ValueError, "Table relation.*does not match.* for 'http'"):
            query.run()

    def test_one_conn_two_tables(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # We will send two tables for this test "http" and "stats".
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(utils.table_id3)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize "http" on the stream.
            http_table1.metadata_response(),
            # Send over a row-batch from "http".
            http_table1.row_batch_response([["foo"], [200]]),
            # Initialize "stats" on the stream.
            stats_table1.metadata_response(),
            # Send over a row-batch from "stats".
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            # Send an end-of-stream for "http".
            http_table1.end(),
            # Send an end-of-stream for "stats".
            stats_table1.end(),
        ])

        query = px_client.query(conns, query_str)
        # Subscribe to both tables.
        http_tb = query.subscribe("http")
        stats_tb = query.subscribe("stats")

        # Async function that makes sure "http" table returns the expected rows.
        async def process_http_tb(table_sub: pixie.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
                self.assertEqual(row["http_resp_body"], "foo")
                self.assertEqual(row["http_resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Async function that makes sure "stats" table returns the expected rows.
        async def process_stats_tb(table_sub: pixie.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
                self.assertEqual(row["upid"], vpb.UInt128(high=123, low=456))
                self.assertEqual(row["cpu_ktime_ns"], 1000)
                self.assertEqual(row["rss_bytes"], 999)
                num_rows += 1

            self.assertEqual(num_rows, 1)
        # Run the query and the processing tasks concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_query_and_tasks(query, [process_http_tb(http_tb), process_stats_tb(stats_tb)]))

    def test_run_script_with_invalid_arg_error(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Send over an error in the Status field. This is the exact error you would
        # get if you sent over an empty pxl function in the ExecuteScriptRequest.
        self.service.add_fake_data(utils.cluster_uuid1, [
            vpb.ExecuteScriptResponse(status=utils.invalid_argument(
                message="Query should not be empty."
            ))
        ])

        # Prepare the query and run synchronously.
        query = px_client.query(conns, "")
        # Although we add a callback, we don't want this to throw an error.
        # Instead the error should be returned by the run function.
        query.add_callback("http_table", lambda row: print(row))
        with self.assertRaisesRegex(ValueError, "Query should not be empty."):
            query.run()

    def test_run_script_with_line_col_error(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Send over an error a line, column error. These kinds of errors come
        # from the compiler pointing to a specific failure in the pxl script.
        self.service.add_fake_data(utils.cluster_uuid1, [
            vpb.ExecuteScriptResponse(status=utils.line_col_error(
                1,
                2,
                message="name 'aa' is not defined"
            ))
        ])

        # Prepare the query and run synchronously.
        query = px_client.query(conns, "aa")
        # Although we add a callback, we don't want this to throw an error.
        # Instead the error should be returned by the run function.
        query.add_callback("http_table", lambda row: print(row))
        with self.assertRaisesRegex(pixie.PxLError, "PxL, line 1.*name 'aa' is not defined"):
            query.run()

    def test_compiler_error_from_two_conns(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [
            FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1),
            FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid2),
        ]

        error_resp = vpb.ExecuteScriptResponse(status=utils.line_col_error(
            1, 2,
            message="name 'aa' is not defined"
        ))
        # Send over an error a line, column error. These kinds of errors come
        # from the compiler pointing to a specific failure in the pxl script.
        self.service.add_fake_data(utils.cluster_uuid1, [error_resp])
        self.service.add_fake_data(utils.cluster_uuid2, [error_resp])

        # Prepare the query and run synchronously.
        query = px_client.query(conns, "aa")
        # Although we add a callback, we don't want this to throw an error.
        # Instead the error should be returned by the run function.
        query.add_callback("http_table", lambda row: print(row))
        with self.assertRaisesRegex(pixie.PxLError, "name 'aa' is not defined"):
            query.run()

    def test_run_script_with_api_errors(self) -> None:
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Only send data for "http".
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            http_table1.end(),
        ])

        query = px_client.query(conns, query_str)

        # Subscribe to a table that doesn't exist shoudl throw an error.
        foobar_tb = query.subscribe("foobar")

        # Try to pull data from the foobar_tb, but error out when the query
        # never produces that data.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, "'foobar' not sent by query"):
            loop.run_until_complete(
                run_query_and_tasks(query, [utils.iterate_and_pass(foobar_tb)]))

    def test_run_script_callback(self) -> None:
        # Test the callback API. Callback API is a simpler alternative to the TableSub
        # API that allows you to designate a function that runs on individual rows. Users
        # can process data without worrying about async processing by using this API.
        px_client = pixie.Client(
            token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Create two tables: "http" and "stats"
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(utils.table_id3)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Init "http".
            http_table1.metadata_response(),
            # Send data for "http".
            http_table1.row_batch_response([["foo"], [200]]),
            # Init "stats".
            stats_table1.metadata_response(),
            # Send data for "stats".
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            # End "http".
            http_table1.end(),
            # End "stats".
            stats_table1.end(),
        ])

        query = px_client.query(conns, query_str)
        http_counter = 0
        stats_counter = 0

        # Define callback function for "http" table.
        def http_fn(row: pixie.Row) -> None:
            nonlocal http_counter
            http_counter += 1
            self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
            self.assertEqual(row["http_resp_body"], "foo")
            self.assertEqual(row["http_resp_status"], 200)
        query.add_callback("http", http_fn)

        # Define a callback function for the stats_fn.
        def stats_fn(row: pixie.Row) -> None:
            nonlocal stats_counter
            stats_counter += 1
            self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
            self.assertEqual(row["upid"], vpb.UInt128(high=123, low=456))
            self.assertEqual(row["cpu_ktime_ns"], 1000)
            self.assertEqual(row["rss_bytes"], 999)
        query.add_callback("stats", stats_fn)

        # Run the query synchronously.
        query.run()

        # We expect each callback function to only be called once.
        self.assertEqual(stats_counter, 1)
        self.assertEqual(http_counter, 1)

    def test_run_script_callback_with_error(self) -> None:
        # Test to demonstrate how errors raised in callbacks can be handled.
        px_client = pixie.Client(
            token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Create HTTP table and add to the stream.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            http_table1.end(),
        ])

        query = px_client.query(conns, query_str)

        # Add callback function that raises an error.
        def http_fn(row: pixie.Row) -> None:
            raise ValueError("random internal error")
        query.add_callback("http", http_fn)

        # Run query synchronously, expecting the internal error to propagate up.
        with self.assertRaisesRegex(ValueError, "random internal error"):
            query.run()

    def test_subscribe_all(self) -> None:
        # Tests `subscribe_all_tables()`.

        px_client = pixie.Client(
            token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        # Create two tables and simulate them sent over as part of the ExecuteScript call.
        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(utils.table_id3)
        self.service.add_fake_data(utils.cluster_uuid1, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            stats_table1.metadata_response(),
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            http_table1.end(),
            stats_table1.end(),
        ])
        # Create query.
        query = px_client.query(conns, query_str)
        # Get a subscription to all of the tables that arrive over the stream.
        tables = query.subscribe_all_tables()

        # Async function to run on the "http" table.
        async def process_http_tb(table_sub: pixie.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["cluster_id"], utils.cluster_uuid1)
                self.assertEqual(row["http_resp_body"], "foo")
                self.assertEqual(row["http_resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Async function that processes the tables subscription and runs the
        # async function above to process the "http" table when that table shows
        # we see thtparticular table.
        async def process_all_tables(tables_gen: pixie.TableSubGenerator) -> None:
            table_names = set()
            async for table in tables_gen:
                table_names.add(table.table_name)
                if table.table_name == "http":
                    # Once we find the http_tb, process it.
                    await process_http_tb(table)
            # Make sure we see both tables on the generator.
            self.assertEqual(table_names, {"http", "stats"})

        # Run the query and process_all_tables function concurrently.
        # We expect no errors.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_query_and_tasks(query, [process_all_tables(tables())]))

    def test_subscribe_same_table_twice(self) -> None:
        # Only on subscription allowed per table. Users should handle data from
        # the single alloatted subscription to enable the logical equivalent
        # of multiple subscriptions to one table.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        query = px_client.query(conns, query_str)

        # First subscription is fine.
        query.subscribe("http")

        # Second raises an error.
        with self.assertRaisesRegex(ValueError, "Already subscribed to 'http'"):
            query.subscribe("http")

    def test_fail_on_multi_run(self) -> None:
        # Tests to show that queries may only be run once. After a query has been
        # run, calling data grabbing methods like subscribe, add_callback, etc. will
        # raise an error.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        stats_table1 = self.stats_table_factory.create_table(utils.table_id3)
        self.service.add_fake_data(utils.cluster_uuid1, [
            stats_table1.metadata_response(),
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            stats_table1.end(),
        ])

        query = px_client.query(conns, query_str)

        # Create a dummy callback.
        def stats_cb(row: pixie.Row) -> None:
            pass

        query.add_callback("stats", stats_cb)
        # Run the query for the first time. Should not return an error.
        query.run()
        # Each of the following methods should fail if called after query.run()
        query_ran_message = "Query already ran"
        # Adding a callback should fail.
        with self.assertRaisesRegex(ValueError, query_ran_message):
            query.add_callback("stats", stats_cb)
        # Subscribing to a table should fail.
        with self.assertRaisesRegex(ValueError, query_ran_message):
            query.subscribe("stats")
        # Subscribing to all tables should fail.
        with self.assertRaisesRegex(ValueError, query_ran_message):
            query.subscribe_all_tables()
        # Synchronous run should error out.
        with self.assertRaisesRegex(ValueError, query_ran_message):
            query.run()
        # Async run should error out.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, query_ran_message):
            loop.run_until_complete(query.run_async())

    def test_send_error_id_table_prop(self) -> None:
        # Sending an error over the stream should cause the table sub to exit.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to a single fake cluster.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream and send over a rowbatch.
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            # Send over an error on the stream after we've started sending data.
            # this should happen if something breaks on the Pixie side.
            # Note: the table does not send an end message over the stream.
            vpb.ExecuteScriptResponse(status=utils.invalid_argument(
                message="server error"
            ))
        ])

        # Create the query object.
        query = px_client.query(conns, query_str)
        # Add callback for http table.
        query.add_callback("http", lambda _: None)

        with self.assertRaisesRegex(ValueError, "server error"):
            query.run()

    def test_stop_sending_data_before_eos(self) -> None:
        # If the stream stops before sending over an eos for each table that should be an error.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to a single fake cluster.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream and send over a rowbatch.
            http_table1.metadata_response(),
            http_table1.row_batch_response([["foo"], [200]]),
            # Note: the table does not send an end message over the stream.
        ])

        # Create the query object.
        query = px_client.query(conns, query_str)
        # Subscribe to the http table.
        http_tb = query.subscribe("http")

        # Run the query and process_table concurrently.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, "More clusters closed than received eos."):
            loop.run_until_complete(
                run_query_and_tasks(query, [utils.iterate_and_pass(http_tb)]))

    def test_handle_server_side_errors(self) -> None:
        # Test to make sure server side errors are handled somewhat.
        px_client = pixie.Client(token=ACCESS_TOKEN, server_url=self.url())
        # Connect to a single fake cluster.
        conns = [FakeConn(ACCESS_TOKEN, self.url(), utils.cluster_uuid1)]

        http_table1 = self.http_table_factory.create_table(utils.table_id1)
        self.service.add_fake_data(utils.cluster_uuid1, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([["foo"], [200]]),
            # NOTE: don't send over the eos -> simulating error midway through
            # stream.

        ])
        self.service.trigger_error(utils.cluster_uuid1, ValueError('hi'))

        # Create the query object.
        query = px_client.query(conns, query_str)
        # Subscribe to the http table.
        http_tb = query.subscribe("http")

        # Run the query and process_table concurrently.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(grpc.aio.AioRpcError, "hi"):
            loop.run_until_complete(
                run_query_and_tasks(query, [utils.iterate_and_pass(http_tb)]))


if __name__ == "__main__":
    unittest.main()
