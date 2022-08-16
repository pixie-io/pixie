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
import unittest
import uuid

from concurrent import futures
from typing import List, Any, Coroutine, Dict

from pxapi import cloudapi_pb2_grpc, cpb, vizierapi_pb2_grpc, vpb, utils
import pxapi

import test_utils

ACCESS_TOKEN = "12345678-0000-0000-0000-987654321012"
pxl_script = """
import px
px.display(px.DataFrame('http_events')[
           ['resp_body','resp_status']].head(10), 'http')
px.display(px.DataFrame('process_stats')[
           ['upid','cpu_ktime_ns', 'rss_bytes']].head(10), 'stats')
"""


async def run_script_and_tasks(
    script_executor: pxapi.ScriptExecutor,
    processors: List[Coroutine[Any, Any, Any]]
) -> None:
    """
    Runs data processors in parallel with a script_executor, returns the result of the coroutine.
    """
    tasks = [asyncio.create_task(p) for p in processors]
    try:
        await script_executor.run_async()
    except Exception as e:
        for t in tasks:
            t.cancel()
        raise e

    await asyncio.gather(*tasks)


class VizierServiceFake(vizierapi_pb2_grpc.VizierServiceServicer):
    def __init__(self) -> None:
        self.cluster_id_to_fake_data: Dict[str,
                                           List[test_utils.ExecResponse]] = {}
        self.cluster_id_to_error: Dict[str, Exception] = {}

    def add_fake_data(self, cluster_id: str, data: List[test_utils.ExecResponse]) -> None:
        if cluster_id not in self.cluster_id_to_fake_data:
            self.cluster_id_to_fake_data[cluster_id] = []
        self.cluster_id_to_fake_data[cluster_id].extend(data)

    def trigger_error(self, cluster_id: str, exc: Exception) -> None:
        """ Adds an error that triggers after the data is yielded. """
        self.cluster_id_to_error[cluster_id] = exc

    def ExecuteScript(self, request: vpb.ExecuteScriptRequest, context: Any) -> Any:
        cluster_id = request.cluster_id
        assert cluster_id in self.cluster_id_to_fake_data, f"need data for cluster_id {cluster_id}"
        data = self.cluster_id_to_fake_data[cluster_id]
        opts = None
        if request.HasField("encryption_options"):
            opts = request.encryption_options
        for d in data:
            yield d.encrypted_script_response(opts)

        # Trigger an error for the cluster ID if the user added one.
        if cluster_id in self.cluster_id_to_error:
            raise self.cluster_id_to_error[cluster_id]


def create_cluster_info(
    cluster_id: str,
    cluster_name: str,
    status: cpb.ClusterStatus = cpb.CS_HEALTHY,
) -> cpb.ClusterInfo:
    return cpb.ClusterInfo(
        id=utils.uuid_pb_from_string(cluster_id),
        status=status,
        cluster_name=cluster_name,
    )


class CloudServiceFake(cloudapi_pb2_grpc.VizierClusterInfoServicer):
    def __init__(self) -> None:
        self.clusters = [
            create_cluster_info(
                test_utils.cluster_uuid1,
                "cluster1",
            ),
            create_cluster_info(
                test_utils.cluster_uuid2,
                "cluster2",
            ),
            # One cluster marked as unhealthy.
            create_cluster_info(
                test_utils.cluster_uuid3,
                "cluster3",
                status=cpb.CS_UNHEALTHY,
            ),
        ]

    def GetClusterInfo(
        self,
        request: cpb.GetClusterInfoRequest,
        context: Any,
    ) -> cpb.GetClusterInfoResponse:
        if request.HasField('id'):
            for c in self.clusters:
                if c.id == request.id:
                    return cpb.GetClusterInfoResponse(clusters=[c])
            return cpb.GetClusterInfoResponse(clusters=[])

        return cpb.GetClusterInfoResponse(clusters=self.clusters)


class TestClient(unittest.TestCase):
    def setUp(self) -> None:
        # Create a fake server for the VizierService
        self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=1))
        self.fake_vizier_service = VizierServiceFake()
        self.fake_cloud_service = CloudServiceFake()

        vizierapi_pb2_grpc.add_VizierServiceServicer_to_server(
            self.fake_vizier_service, self.server)

        cloudapi_pb2_grpc.add_VizierClusterInfoServicer_to_server(
            self.fake_cloud_service, self.server)
        self.port = self.server.add_insecure_port("[::]:0")
        self.server.start()

        self.px_client = pxapi.Client(
            token=ACCESS_TOKEN,
            server_url=self.url(),
            channel_fn=lambda url: grpc.insecure_channel(url),
            conn_channel_fn=lambda url: grpc.aio.insecure_channel(url),
        )

        self.http_table_factory = test_utils.FakeTableFactory("http", vpb.Relation(columns=[
            test_utils.string_col("resp_body"),
            test_utils.int64_col("resp_status"),
        ]))

        self.stats_table_factory = test_utils.FakeTableFactory("stats", vpb.Relation(columns=[
            test_utils.uint128_col("upid"),
            test_utils.int64_col("cpu_ktime_ns"),
            test_utils.int64_col("rss_bytes"),
        ]))
        asyncio.set_event_loop(asyncio.new_event_loop())

    def url(self) -> str:
        return f"localhost:{self.port}"

    def tearDown(self) -> None:
        self.server.stop(None)

    def test_list_and_run_healthy_clusters(self) -> None:
        # Tests that users can list healthy clusters and then
        # execute a script on those clusters.

        clusters = self.px_client.list_healthy_clusters()
        self.assertSetEqual(
            set([c.name() for c in clusters]),
            {"cluster1", "cluster2"}
        )

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(clusters[0])

        # Create one http table.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Init "http".
            http_table1.metadata_response(),
            # Send data for "http".
            http_table1.row_batch_response([[b"foo"], [200]]),
            # End "http".
            http_table1.end(),
        ])

        script_executor = self.px_client.connect_to_cluster(
            clusters[0]).prepare_script(pxl_script)

        script_executor.add_callback("http", lambda row: None)

        # Run the script_executor synchronously.
        script_executor.run()

    def test_one_conn_one_table(self) -> None:
        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create table for cluster_uuid1.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([[b"foo"], [200]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Create the script_executor object.
        script_executor = conn.prepare_script(pxl_script)
        # Subscribe to the http table.
        http_tb = script_executor.subscribe("http")

        # Define an async function that processes the TableSub
        # while the API script_executor can run concurrently.
        async def process_table(table_sub: pxapi.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["resp_body"], b"foo")
                self.assertEqual(row["resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Run the script_executor and process_table concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_script_and_tasks(script_executor, [process_table(http_tb)]))

    def test_multiple_rows_and_rowbatches(self) -> None:

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create table for the first cluster.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        rb_data: List[List[Any]] = [
            [b"foo", b"bar", b"baz", b"bat"], [200, 500, 301, 404]]

        # Here we split the above data into two rowbatches.
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Row batch 1 has data 1-3,
            http_table1.row_batch_response([rb_data[0][:3], rb_data[1][:3]]),
            # Row batch 2 has data 4
            http_table1.row_batch_response([rb_data[0][3:], rb_data[1][3:]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Create the script_executor object.
        script_executor = conn.prepare_script(pxl_script)
        # Subscribe to the http table.
        http_tb = script_executor.subscribe("http")

        # Verify that the rows returned by the table_sub match the
        # order and values of the input test data.
        async def process_table(table_sub: pxapi.TableSub) -> None:
            row_i = 0
            # table_sub hides the batched rows and delivers them in
            # the same order as the batches sent.
            async for row in table_sub:
                self.assertEqual(row["resp_body"], rb_data[0][row_i])
                self.assertEqual(row["resp_status"], rb_data[1][row_i])
                row_i += 1

            self.assertEqual(row_i, 4)

        # Run the script_executor and process_table concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_script_and_tasks(script_executor, [process_table(http_tb)]))

    def test_one_conn_two_tables(self) -> None:

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # We will send two tables for this test "http" and "stats".
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(test_utils.table_id3)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize "http" on the stream.
            http_table1.metadata_response(),
            # Send over a row-batch from "http".
            http_table1.row_batch_response([[b"foo"], [200]]),
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

        script_executor = conn.prepare_script(pxl_script)
        # Subscribe to both tables.
        http_tb = script_executor.subscribe("http")
        stats_tb = script_executor.subscribe("stats")

        # Async function that makes sure "http" table returns the expected rows.
        async def process_http_tb(table_sub: pxapi.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["resp_body"], b"foo")
                self.assertEqual(row["resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Async function that makes sure "stats" table returns the expected rows.
        async def process_stats_tb(table_sub: pxapi.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["upid"], uuid.UUID(
                    '00000000-0000-007b-0000-0000000001c8'))
                self.assertEqual(row["cpu_ktime_ns"], 1000)
                self.assertEqual(row["rss_bytes"], 999)
                num_rows += 1

            self.assertEqual(num_rows, 1)
        # Run the script_executor and the processing tasks concurrently.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_script_and_tasks(script_executor, [
                process_http_tb(http_tb),
                process_stats_tb(stats_tb)
            ])
        )

    def test_run_script_with_invalid_arg_error(self) -> None:

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Send over an error in the Status field. This is the exact error you would
        # get if you sent over an empty pxl function in the ExecuteScriptRequest.
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            test_utils.ExecResponse(vpb.ExecuteScriptResponse(status=test_utils.invalid_argument(
                message="Script should not be empty."
            )))
        ])

        # Prepare the script_executor and run synchronously.
        script_executor = conn.prepare_script("")
        # Although we add a callback, we don't want this to throw an error.
        # Instead the error should be returned by the run function.
        script_executor.add_callback("http_table", lambda row: print(row))
        with self.assertRaisesRegex(ValueError, "Script should not be empty."):
            script_executor.run()

    def test_run_script_with_line_col_error(self) -> None:

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Send over an error a line, column error. These kinds of errors come
        # from the compiler pointing to a specific failure in the pxl script_executor.
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            test_utils.ExecResponse(vpb.ExecuteScriptResponse(status=test_utils.line_col_error(
                1,
                2,
                message="name 'aa' is not defined"
            )))
        ])

        # Prepare the script_executor and run synchronously.
        script_executor = conn.prepare_script("aa")
        # Although we add a callback, we don't want this to throw an error.
        # Instead the error should be returned by the run function.
        script_executor.add_callback("http_table", lambda row: print(row))
        with self.assertRaisesRegex(pxapi.PxLError, "PxL, line 1.*name 'aa' is not defined"):
            script_executor.run()

    def test_run_script_with_api_errors(self) -> None:

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Only send data for "http".
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([[b"foo"], [200]]),
            http_table1.end(),
        ])

        script_executor = conn.prepare_script(pxl_script)

        # Subscribe to a table that doesn't exist shoudl throw an error.
        foobar_tb = script_executor.subscribe("foobar")

        # Try to pull data from the foobar_tb, but error out when the script_executor
        # never produces that data.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, "Table 'foobar' not received"):
            loop.run_until_complete(
                run_script_and_tasks(script_executor, [test_utils.iterate_and_pass(foobar_tb)]))

    def test_run_script_callback(self) -> None:
        # Test the callback API. Callback API is a simpler alternative to the TableSub
        # API that allows you to designate a function that runs on individual rows. Users
        # can process data without worrying about async processing by using this API.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create two tables: "http" and "stats"
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(test_utils.table_id3)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Init "http".
            http_table1.metadata_response(),
            # Send data for "http".
            http_table1.row_batch_response([[b"foo"], [200]]),
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

        script_executor = conn.prepare_script(pxl_script)
        http_counter = 0
        stats_counter = 0

        # Define callback function for "http" table.
        def http_fn(row: pxapi.Row) -> None:
            nonlocal http_counter
            http_counter += 1
            self.assertEqual(row["resp_body"], b"foo")
            self.assertEqual(row["resp_status"], 200)
        script_executor.add_callback("http", http_fn)

        # Define a callback function for the stats_fn.
        def stats_fn(row: pxapi.Row) -> None:
            nonlocal stats_counter
            stats_counter += 1
            self.assertEqual(row["upid"], uuid.UUID(
                '00000000-0000-007b-0000-0000000001c8'))
            self.assertEqual(row["cpu_ktime_ns"], 1000)
            self.assertEqual(row["rss_bytes"], 999)
        script_executor.add_callback("stats", stats_fn)

        # Run the script_executor synchronously.
        script_executor.run()

        # We expect each callback function to only be called once.
        self.assertEqual(stats_counter, 1)
        self.assertEqual(http_counter, 1)

    def test_run_script_callback_with_error(self) -> None:
        # Test to demonstrate how errors raised in callbacks can be handled.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create HTTP table and add to the stream.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([[b"foo"], [200]]),
            http_table1.end(),
        ])

        script_executor = conn.prepare_script(pxl_script)

        # Add callback function that raises an error.
        def http_fn(row: pxapi.Row) -> None:
            raise ValueError("random internal error")
        script_executor.add_callback("http", http_fn)

        # Run script_executor synchronously, expecting the internal error to propagate up.
        with self.assertRaisesRegex(ValueError, "random internal error"):
            script_executor.run()

    def test_subscribe_all(self) -> None:
        # Tests `subscribe_all_tables()`.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create two tables and simulate them sent over as part of the ExecuteScript call.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        stats_table1 = self.stats_table_factory.create_table(test_utils.table_id3)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            http_table1.metadata_response(),
            http_table1.row_batch_response([[b"foo"], [200]]),
            stats_table1.metadata_response(),
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            http_table1.end(),
            stats_table1.end(),
        ])
        # Create script_executor.
        script_executor = conn.prepare_script(pxl_script)
        # Get a subscription to all of the tables that arrive over the stream.
        tables = script_executor.subscribe_all_tables()

        # Async function to run on the "http" table.
        async def process_http_tb(table_sub: pxapi.TableSub) -> None:
            num_rows = 0
            async for row in table_sub:
                self.assertEqual(row["resp_body"], b"foo")
                self.assertEqual(row["resp_status"], 200)
                num_rows += 1

            self.assertEqual(num_rows, 1)

        # Async function that processes the tables subscription and runs the
        # async function above to process the "http" table when that table shows
        # we see thtparticular table.
        async def process_all_tables(tables_gen: pxapi.TableSubGenerator) -> None:
            table_names = set()
            async for table in tables_gen:
                table_names.add(table.table_name)
                if table.table_name == "http":
                    # Once we find the http_tb, process it.
                    await process_http_tb(table)
            # Make sure we see both tables on the generator.
            self.assertEqual(table_names, {"http", "stats"})

        # Run the script_executor and process_all_tables function concurrently.
        # We expect no errors.
        loop = asyncio.get_event_loop()
        loop.run_until_complete(
            run_script_and_tasks(script_executor, [process_all_tables(tables())]))

    def test_subscribe_same_table_twice(self) -> None:
        # Only on subscription allowed per table. Users should handle data from
        # the single alloatted subscription to enable the logical equivalent
        # of multiple subscriptions to one table.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        script_executor = conn.prepare_script(pxl_script)

        # First subscription is fine.
        script_executor.subscribe("http")

        # Second raises an error.
        with self.assertRaisesRegex(ValueError, "Already subscribed to 'http'"):
            script_executor.subscribe("http")

    def test_fail_on_multi_run(self) -> None:
        # Tests to show that queries may only be run once. After a script_executor has been
        # run, calling data grabbing methods like subscribe, add_callback, etc. will
        # raise an error.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        stats_table1 = self.stats_table_factory.create_table(test_utils.table_id3)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            stats_table1.metadata_response(),
            stats_table1.row_batch_response([
                [vpb.UInt128(high=123, low=456)],
                [1000],
                [999],
            ]),
            stats_table1.end(),
        ])

        script_executor = conn.prepare_script(pxl_script)

        # Create a dummy callback.
        def stats_cb(row: pxapi.Row) -> None:
            pass

        script_executor.add_callback("stats", stats_cb)
        # Run the script_executor for the first time. Should not return an error.
        script_executor.run()

        # Each of the following methods should fail if called after script_executor.run()
        script_ran_message = "Script already executed"
        # Adding a callback should fail.
        with self.assertRaisesRegex(ValueError, script_ran_message):
            script_executor.add_callback("stats", stats_cb)
        # Subscribing to a table should fail.
        with self.assertRaisesRegex(ValueError, script_ran_message):
            script_executor.subscribe("stats")
        # Subscribing to all tables should fail.
        with self.assertRaisesRegex(ValueError, script_ran_message):
            script_executor.subscribe_all_tables()
        # Synchronous run should error out.
        with self.assertRaisesRegex(ValueError, script_ran_message):
            script_executor.run()
        # Async run should error out.
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        with self.assertRaisesRegex(ValueError, script_ran_message):
            loop.run_until_complete(script_executor.run_async())

    def test_send_error_id_table_prop(self) -> None:
        # Sending an error over the stream should cause the table sub to exit.

        # Connect to a single fake cluster.
        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream and send over a rowbatch.
            http_table1.metadata_response(),
            http_table1.row_batch_response([[b"foo"], [200]]),
            # Send over an error on the stream after we've started sending data.
            # this should happen if something breaks on the Pixie side.
            # Note: the table does not send an end message over the stream.
            test_utils.ExecResponse(vpb.ExecuteScriptResponse(status=test_utils.invalid_argument(
                message="server error"
            ))),
        ])

        # Create the script_executor object.
        script_executor = conn.prepare_script(pxl_script)
        # Add callback for http table.
        script_executor.add_callback("http", lambda _: None)

        with self.assertRaisesRegex(ValueError, "server error"):
            script_executor.run()

    def test_stop_sending_data_before_eos(self) -> None:
        # If the stream stops before sending over an eos for each table that should be an error.

        # Connect to a single fake cluster.
        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream and send over a rowbatch.
            http_table1.metadata_response(),
            http_table1.row_batch_response([[b"foo"], [200]]),
            # Note: the table does not send an end message over the stream.
        ])

        # Create the script_executor object.
        script_executor = conn.prepare_script(pxl_script)
        # Subscribe to the http table.
        http_tb = script_executor.subscribe("http")

        # Run the script_executor and process_table concurrently.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, "Closed before receiving end-of-stream."):
            loop.run_until_complete(
                run_script_and_tasks(script_executor, [test_utils.iterate_and_pass(http_tb)]))

    def test_handle_server_side_errors(self) -> None:
        # Test to make sure server side errors are handled somewhat.

        # Connect to a single fake cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([[b"foo"], [200]]),
            # NOTE: don't send over the eos -> simulating error midway through
            # stream.

        ])
        self.fake_vizier_service.trigger_error(
            test_utils.cluster_uuid1, ValueError('hi'))
        # Create the script_executor object.
        script_executor = conn.prepare_script(pxl_script)
        # Subscribe to the http table.
        http_tb = script_executor.subscribe("http")

        # Run the script_executor and process_table concurrently.
        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(grpc.aio.AioRpcError, "hi"):
            loop.run_until_complete(
                run_script_and_tasks(script_executor, [test_utils.iterate_and_pass(http_tb)]))

    def test_ergo_api(self) -> None:
        # Create a new API where we can run and get results for a table simultaneously.
        # Connect to a cluster.
        conn = self.px_client.connect_to_cluster(
            self.px_client.list_healthy_clusters()[0])

        # Create the script_executor.
        script_executor = conn.prepare_script(pxl_script)

        # Create table for cluster_uuid1.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([[b"foo"], [200]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Use the results API to run and get the data from the http table.
        for row in script_executor.results("http"):
            self.assertEqual(row["resp_body"], b"foo")
            self.assertEqual(row["resp_status"], 200)

    def test_shared_grpc_channel_for_cloud(self) -> None:
        # Make sure the shraed grpc channel are actually shared.
        num_create_channel_calls = 0

        def cloud_channel_fn(url: str) -> grpc.Channel:
            # Nonlocal because we're incrementing the outer variable.
            nonlocal num_create_channel_calls
            num_create_channel_calls += 1
            return grpc.insecure_channel(url)

        px_client = pxapi.Client(
            token=ACCESS_TOKEN,
            server_url=self.url(),
            # Channel functions for testing.
            channel_fn=cloud_channel_fn,
            conn_channel_fn=lambda url: grpc.aio.insecure_channel(url),
        )

        # Connect to a cluster.
        healthy_clusters = px_client.list_healthy_clusters()
        self.assertEqual(num_create_channel_calls, 1)

        # No new channels made on later calls to cloud
        px_client.connect_to_cluster(healthy_clusters[0])
        self.assertEqual(num_create_channel_calls, 1)

        # Reset cache, so now must create a new channel.
        px_client._cloud_channel_cache = None
        px_client.connect_to_cluster(healthy_clusters[0])
        self.assertEqual(num_create_channel_calls, 2)

    def test_encryption(self) -> None:
        # Test creating encrypted clients.
        px_client = pxapi.Client(
            token=ACCESS_TOKEN,
            server_url=self.url(),
            use_encryption=True,
            channel_fn=lambda url: grpc.insecure_channel(url),
            conn_channel_fn=lambda url: grpc.aio.insecure_channel(url),
        )
        conn = px_client.connect_to_cluster(
            px_client.list_healthy_clusters()[0])

        # Create the script_executor.
        script_executor = conn.prepare_script(pxl_script)

        self.assertTrue(script_executor._use_encryption)

        # Create table for cluster_uuid1.
        http_table1 = self.http_table_factory.create_table(test_utils.table_id1)
        self.fake_vizier_service.add_fake_data(conn.cluster_id, [
            # Initialize the table on the stream with the metadata.
            http_table1.metadata_response(),
            # Send over a single-row batch.
            http_table1.row_batch_response([[b"foo"], [200]]),
            # Send an end-of-stream for the table.
            http_table1.end(),
        ])

        # Use the results API to run and get the data from the http table.
        for row in script_executor.results("http"):
            self.assertEqual(row["resp_body"], b"foo")
            self.assertEqual(row["resp_status"], 200)


if __name__ == "__main__":
    unittest.main()
