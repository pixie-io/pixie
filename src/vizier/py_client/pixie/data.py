import json
import uuid
import asyncio
from typing import Callable, Any, List, AsyncGenerator, Set

from collections import OrderedDict

from src.vizier.vizierpb import vizier_pb2 as vpb


# Function that transforms a Column, a oneof field in the proto, into the specific data type.
ColumnFn = Callable[[vpb.Column], Any]

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

        if column_type == vpb.TIME64NS:
            return lambda x: x.time64ns_data
        if column_type == vpb.FLOAT64:
            return lambda x: x.float64_data
        if column_type == vpb.INT64:
            return lambda x: x.int64_data
        if column_type == vpb.STRING:
            return lambda x: x.string_data
        if column_type == vpb.UINT128:
            return lambda x: x.uint128_data
        if column_type == vpb.BOOLEAN:
            return lambda x: x.boolean_data
        raise ValueError("{} type not supported".format(column_type))
        return lambda x: ''

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

    def __getitem__(self, column: str) -> Any:
        """
        Returns the value for the specified column.

        Raises:
            KeyError: If `column` does not exist in `self.relation`.
        """
        idx = self.relation.get_key_idx(column)
        if idx == -1:
            raise KeyError("'{}' not found in relation".format(column))
        return self._data[idx]

    def __str__(self) -> str:
        out = OrderedDict()
        for i, c in enumerate(self._data):
            out[self.relation.get_col_name(i)] = c
        return json.dumps(out, indent=2, cls=_UInt128Encoder)


RowGenerator = AsyncGenerator[Row, None]


class _Rowbatch:
    def __init__(self, cluster_id: ClusterID, rb: vpb.RowBatchData, close_table: bool = False):
        self.batch = rb
        self.cluster_id = cluster_id
        self.close_table = close_table


def _create_cluster_id_col() -> vpb.Relation.ColumnInfo:
    # Create a cluster_id column.
    cluster_id_col = vpb.Relation.ColumnInfo()
    cluster_id_col.column_name = "cluster_id"
    cluster_id_col.column_type = vpb.STRING
    cluster_id_col.column_desc = "ID of the cluster that this came from."
    cluster_id_col.column_semantic_type = vpb.ST_NONE
    return cluster_id_col


def _add_cluster_id_to_relation(relation: vpb.Relation) -> vpb.Relation:
    cluster_id_relation = vpb.Relation(columns=relation.columns)
    cluster_id_relation.columns.insert(0, _create_cluster_id_col())
    return cluster_id_relation


class _TableStream:
    def __init__(self, name: str, relation: _Relation, expected_num_conns: int, subscribed: bool):
        self.name = name
        self.relation = relation

        self.table_ids: List[TableID] = []
        self.cluster_ids: List[ClusterID] = []
        # Tells you how many clusters you expect to get data from.
        # Added because len(self.cluster_ids) is unreliable, you might finish
        # getting data from one cluster before adding another cluster so you would
        # exit a table early.
        self.expected_num_conns = expected_num_conns
        # From how many clusters have sent eos. Use to stop yielding rowbatches.
        self.received_eos = 0

        # The clusters that have called close().
        self._closed_clusters: Set[ClusterID] = set()

        self._rowbatch_q: asyncio.Queue[_Rowbatch] = asyncio.Queue()
        self._subscribed = subscribed

    def add_cluster_table_id(self, table_id: TableID, cluster_id: ClusterID) -> None:
        self.table_ids.append(table_id)
        self.cluster_ids.append(cluster_id)

        if len(self.cluster_ids) > self.expected_num_conns:
            raise ValueError(
                "Shouldn't have more clusters than expected number of connections.")

    def add_row_batch(self, cluster_id: ClusterID, rowbatch: vpb.RowBatchData) -> None:
        if not self._subscribed:
            return
        self._rowbatch_q.put_nowait(_Rowbatch(cluster_id, rowbatch))

    def close(self, cluster_id: ClusterID) -> None:
        self._rowbatch_q.put_nowait(
            _Rowbatch(cluster_id, vpb.RowBatchData(), close_table=True))

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
                self._closed_clusters.add(rb.cluster_id)
                # Should only happen if we haven't received an
                # `eos` from each cluster_id before close(cluster_id).
                if len(self._closed_clusters) == self.expected_num_conns:
                    raise ValueError("More clusters closed than received eos.")
                continue
            yield rb
            self._rowbatch_q.task_done()

            if rb.batch.eos:
                self.received_eos += 1
            # If we've received enough eos, then we end the generator.
            if self.received_eos == self.expected_num_conns:
                break

    async def __aiter__(self) -> RowGenerator:
        async for rb in self._row_batches():
            batch = rb.batch
            for i in range(batch.num_rows):
                row = [rb.cluster_id]
                for ci, col in enumerate(batch.cols):
                    # Adding + 1 because the batches don't contain a cluster ID, but our relation
                    # for this table does contain one.
                    format_fn = self.relation.get_col_formatter(ci + 1)
                    row.append(format_fn(col).data[i])
                yield Row(self, row)
