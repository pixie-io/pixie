import unittest
import json
import asyncio
from typing import List, Any


import data
from data import vpb
import test_utils as utils


class TestData(unittest.TestCase):
    def setUp(self) -> None:
        self.relation = vpb.Relation(columns=[
            utils.string_col("http_resp_body"),
            utils.int64_col("http_resp_status"),
        ])

    def test_row(self) -> None:
        table = data._TableStream("foo", data._Relation(self.relation),
                                  subscribed=True)

        # Test __str__().
        row = data.Row(table, ["bar", 200])
        self.assertEqual(json.loads(str(row)), {
            "http_resp_body": "bar",
            "http_resp_status": 200
        })

        # Make sure you can index the columns.
        self.assertEqual(row[0], "bar")
        self.assertEqual(row[1], 200)

        # Make sure you can access the columns by name.
        self.assertEqual(row["http_resp_body"], "bar")
        self.assertEqual(row["http_resp_status"], 200)

        # Grabbing a column that doesn't exist should fail.
        with self.assertRaisesRegex(KeyError, ".* not found in relation"):
            row["baz"]

        with self.assertRaisesRegex(IndexError, ""):
            row[2]

        with self.assertRaisesRegex(KeyError, "Unexpected key type"):
            row[2.2]

        # Creating a row that has wrong number of columns should fail.
        with self.assertRaisesRegex(ValueError, "Mismatch of row length 3 and relation size 2"):
            data.Row(table, ["bar", 200, "extra"])

    def test_table_stream(self) -> None:
        # Create the table stream.
        table = data._TableStream("foo",
                                  data._Relation(
                                      self.relation,
                                  ),
                                  subscribed=True)

        foo_factory = utils.FakeTableFactory("foo", self.relation)
        foo_faker = foo_factory.create_table(utils.table_id1)

        rb_data: List[List[Any]] = [
            ["foo", "bar", "baz", "bat"], [200, 500, 301, 404]]
        # A data rowbatch.
        batch1 = foo_faker.row_batch(rb_data)

        # The end of table row batch.
        batch2 = foo_faker.row_batch([[], []], eos=True, eow=True)

        # Push the rowbatches onto this table stream.
        table.add_row_batch(batch1)
        table.add_row_batch(batch2)

        async def process_rows() -> None:
            i = 0
            async for row in table:
                self.assertEqual(row["http_resp_body"], rb_data[0][i])
                self.assertEqual(row["http_resp_status"], rb_data[1][i])
                i += 1

        loop = asyncio.get_event_loop()
        loop.run_until_complete(process_rows())

    def test_unsubbed_table_stream(self) -> None:
        # Create the table stream, but it should be unsubscribed.
        table = data._TableStream("foo",
                                  data._Relation(
                                      self.relation,
                                  ),
                                  subscribed=False)

        foo_factory = utils.FakeTableFactory("foo", self.relation)
        foo_faker = foo_factory.create_table(utils.table_id1)

        rb_data: List[List[Any]] = [
            ["foo", "bar", "baz", "bat"], [200, 500, 301, 404]]
        # A data rowbatch.
        batch1 = foo_faker.row_batch(rb_data)

        # The end of table row batch.
        batch2 = foo_faker.row_batch([[], []], eos=True, eow=True)

        # Push the rowbatches onto this table stream.
        table.add_row_batch(batch1)
        table.add_row_batch(batch2)

        loop = asyncio.get_event_loop()
        with self.assertRaisesRegex(ValueError, "Table .* not subscribed"):
            loop.run_until_complete(utils.iterate_and_pass(table))


if __name__ == "__main__":
    unittest.main()
