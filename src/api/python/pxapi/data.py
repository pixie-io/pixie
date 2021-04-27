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
import json
import uuid

from collections import OrderedDict
from typing import Callable, Any, List, AsyncGenerator, Union

from src.api.proto.vizierpb import vizierapi_pb2 as vpb


# Function that transforms a Column (a oneof field in the proto)
# and row index into the specific data type.
ColumnFn = Callable[[vpb.Column, int], Any]

# The ID of the cluster.
ClusterID = str

# The ID of the table from a particular connection.
TableID = str


class _Relation:
    def __init__(self, relation: vpb.Relation) -> None:
        self._columns = relation.columns
        self._col_formatter_cache: List[ColumnFn] = []
        self._create_col_formatters()

    def get_key_idx(self, key: str) -> int:
        for idx, k in enumerate(self._columns):
            if self._columns[idx].column_name == key:
                return idx

        return -1

    def _col_formatter_impl(self, idx: int) -> ColumnFn:
        column = self._columns[idx]
        column_type = column.column_type

        # Internal function that will get called by all
        # Column getters.
        def get_i(col: Any, i: int) -> Any:
            return col.data[i]

        if column_type == vpb.TIME64NS:
            return lambda x, i: get_i(x.time64ns_data, i)
        if column_type == vpb.FLOAT64:
            return lambda x, i: get_i(x.float64_data, i)
        if column_type == vpb.INT64:
            return lambda x, i: get_i(x.int64_data, i)
        if column_type == vpb.STRING:
            return lambda x, i: get_i(x.string_data, i)
        if column_type == vpb.UINT128:
            # Encode the UINT128 as UUIDs to be readable.
            return lambda x, i: _encode_uint128_as_UUID(get_i(x.uint128_data, i))
        if column_type == vpb.BOOLEAN:
            return lambda x, i: get_i(x.boolean_data, i)
        raise ValueError("{} type not supported".format(column_type))
        return lambda x, i: ''

    def _create_col_formatters(self) -> None:
        for i in range(len(self._columns)):
            self._col_formatter_cache.append(self._col_formatter_impl(i))

    def get_col_formatter(self, idx: int) -> ColumnFn:
        return self._col_formatter_cache[idx]

    def num_cols(self) -> int:
        return len(self._columns)

    def get_col_name(self, idx: int) -> str:
        return self._columns[idx].column_name

    def __eq__(self, other: Any) -> bool:
        if not issubclass(type(other), _Relation):
            return False

        if len(self._columns) != len(other._columns):
            return False

        for left, right in zip(self._columns, other._columns):
            if left != right:
                return False

        return True


def _encode_uint128_as_UUID(uint128: vpb.UInt128) -> uuid.UUID:
    def int_to_bytes(x: int) -> bytes:
        return x.to_bytes(8, byteorder='big')
    return uuid.UUID(bytes=int_to_bytes(uint128.high) + int_to_bytes(uint128.low))


class _UInt128Encoder(json.JSONEncoder):
    def default(self, o: Any) -> str:
        if isinstance(o, vpb.UInt128):
            return str(_encode_uint128_as_UUID(o))
        return json.JSONEncoder().encode(o)


class Row:
    """
    Row represents a row of data for a particular table. You can easily access
    data in the row by using the column name from the associated table.

    Specifically designed to avoid allocation memory for the relation for each row.

    Examples:
      >>> tableA = Table("a", relation=(("cola",int), ("colb", int), ("colc", string)))
      >>> row = Row(tableA, [1,2,"three"])
      >>> row["cola"]
      1
      >>> row["colb"]
      2
      >>> row["colc"]
      "three"
      >>> row
      { "cola": 1, "colb": 2, "colc": "three" }

    """

    def __init__(self, table: '_TableStream', data: List[Any]):
        self._data = data
        self.relation = table.relation
        if len(self._data) != self.relation.num_cols():
            raise ValueError('Mismatch of row length {} and relation size {}'.format(
                len(self._data), self.relation.num_cols()))

    def __getitem__(self, column: Union[str, int]) -> Any:
        """
        Returns the value for the specified column. Can specify column by name or by index.

        Raises:
            KeyError: If `column` does not exist in `self.relation` (if hte )
        """
        if isinstance(column, str):
            idx = self.relation.get_key_idx(column)
            if idx == -1:
                raise KeyError("'{}' not found in relation".format(column))
        elif isinstance(column, int):
            idx = column
        else:
            raise KeyError(
                f"Unexpected key type for 'column': {type(column)}")

        return self._data[idx]

    def __str__(self) -> str:
        out = OrderedDict()
        for i, c in enumerate(self._data):
            out[self.relation.get_col_name(i)] = c
        return json.dumps(out, indent=2, cls=_UInt128Encoder)


RowGenerator = AsyncGenerator[Row, None]


class _Rowbatch:
    def __init__(self, rb: vpb.RowBatchData, close_table: bool = False):
        self.batch = rb
        self.close_table = close_table


class _TableStream:
    def __init__(self, name: str, relation: _Relation, subscribed: bool):
        self.name = name
        self.relation = relation

        self._rowbatch_q: asyncio.Queue[_Rowbatch] = asyncio.Queue()
        self._subscribed = subscribed

    def add_row_batch(self, rowbatch: vpb.RowBatchData) -> None:
        if not self._subscribed:
            return
        self._rowbatch_q.put_nowait(_Rowbatch(rowbatch))

    def close(self) -> None:
        self._rowbatch_q.put_nowait(
            _Rowbatch(vpb.RowBatchData(), close_table=True))

    async def _row_batches(self) -> AsyncGenerator[_Rowbatch, None]:
        if not self._subscribed:
            raise ValueError("Table '{}' not subscribed.".format(self.name))
        # Get the batch from somewhere, continue yielding until done.
        while True:
            rb: _Rowbatch = await self._rowbatch_q.get()
            # rb.close_table is how we communicate whether a connection has been closed
            # for a particular cluster. If this happens before we receive eos from each stream,
            # we throw an error.
            if rb.close_table:
                raise ValueError("Closed before receiving end-of-stream.")
            yield rb
            self._rowbatch_q.task_done()

            if rb.batch.eos:
                break

    async def __aiter__(self) -> RowGenerator:
        async for rb in self._row_batches():
            batch = rb.batch
            for i in range(batch.num_rows):
                row = []
                for ci, col in enumerate(batch.cols):
                    format_fn = self.relation.get_col_formatter(ci)
                    row.append(format_fn(col, i))
                yield Row(self, row)
