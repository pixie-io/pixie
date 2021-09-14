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

import grpc
from typing import List, Any
import json
from authlib.jose import JsonWebKey, JsonWebEncryption

from pxapi import vpb
import pxapi


cluster_uuid1 = "10000000-0000-0000-0000-000000000001"
cluster_uuid2 = "10000000-0000-0000-0000-000000000002"
cluster_uuid3 = "10000000-0000-0000-0000-000000000003"

table_id1 = "20000000-0000-0000-0000-000000000001"
table_id2 = "20000000-0000-0000-0000-000000000002"
table_id3 = "20000000-0000-0000-0000-000000000003"
table_id4 = "20000000-0000-0000-0000-000000000004"


def _create_col(
        column_type: vpb.DataType,
        column_name: str,
        column_desc: str = "",
        sem_type: vpb.SemanticType = vpb.ST_NONE) -> vpb.Relation.ColumnInfo:
    return vpb.Relation.ColumnInfo(
        column_name=column_name,
        column_type=column_type,
        column_desc=column_desc,
        column_semantic_type=sem_type,
    )


def string_col(column_name: str) -> vpb.Relation.ColumnInfo:
    return _create_col(column_type=vpb.STRING, column_name=column_name)


def int64_col(column_name: str) -> vpb.Relation.ColumnInfo:
    return _create_col(column_type=vpb.INT64, column_name=column_name)


def uint128_col(column_name: str) -> vpb.Relation.ColumnInfo:
    return _create_col(
        column_type=vpb.UINT128,
        column_name=column_name,
        sem_type=vpb.ST_UPID,
    )


def _ok() -> vpb.Status:
    return vpb.Status(code=0)


def _compiler_error(line: int, col: int, message: str) -> vpb.ErrorDetails:
    return vpb.ErrorDetails(compiler_error=vpb.CompilerError(
        line=line,
        column=col,
        message=message
    ))


def invalid_argument(
        message: str = "",
        error_details: List[vpb.ErrorDetails] = []) -> vpb.Status:
    return vpb.Status(
        code=grpc.StatusCode.INVALID_ARGUMENT.value[0],
        message=message,
        error_details=error_details
    )


def line_col_error(line: int, col: int, message: str) -> vpb.Status:
    return invalid_argument(
        error_details=[
            _compiler_error(
                line,
                col,
                message)
        ]
    )


def make_upid() -> vpb.UInt128:
    return vpb.UInt128(high=123, low=456)


def _make_column(column: List[Any], coltype: vpb.DataType) -> vpb.Column:
    if coltype == vpb.BOOLEAN:
        return vpb.Column(boolean_data=vpb.BooleanColumn(data=column))
    elif coltype == vpb.INT64:
        return vpb.Column(int64_data=vpb.Int64Column(data=column))
    elif coltype == vpb.UINT128:
        return vpb.Column(uint128_data=vpb.UInt128Column(data=column))
    elif coltype == vpb.FLOAT64:
        return vpb.Column(float64_data=vpb.Float64Column(data=column))
    elif coltype == vpb.STRING:
        return vpb.Column(string_data=vpb.StringColumn(data=column))
    elif coltype == vpb.TIME64NS:
        return vpb.Column(time64ns_data=vpb.Time64NSColumn(data=column))
    else:
        raise ValueError(f"Coltype {coltype} not handled")


class FakeTableFactory:
    def __init__(self, name: str, relation: vpb.Relation):
        self.name = name
        self.relation = relation

    def create_table(self, table_id: str) -> "FakeTable":
        return FakeTable(self.name, self.relation, table_id)


class ExecResponse:
    def __init__(self, data: vpb.ExecuteScriptResponse):
        self.execute_script_response = data

    def encrypted_script_response(self, opts: vpb.ExecuteScriptRequest.EncryptionOptions) -> vpb.ExecuteScriptResponse:
        '''
        Returns a script repsonse with encrypted row batch fields, if they exist,
        and if the options are set.
        '''
        es_resp = self.execute_script_response

        if not es_resp.HasField("data"):
            return es_resp
        if opts is None:
            return es_resp

        # Now we encrypt the batch.
        rb = es_resp.data.batch.SerializeToString()
        key = JsonWebKey.import_key(json.loads(opts.jwk_key))
        encrypted_batch = JsonWebEncryption().serialize_compact({
            'alg': opts.key_alg,
            'enc': opts.content_alg,
            'zip': opts.compression_alg,
        }, rb, key)

        # Make sure we only send the encrypted_batch through.
        es_resp.data.ClearField('batch')
        es_resp.data.encrypted_batch = encrypted_batch
        return es_resp


class FakeTable:
    def __init__(self, name: str, relation: vpb.Relation, id: str):
        self.name = name
        self.relation = relation
        self.id = id

    def _metadata(self) -> vpb.QueryMetadata:
        return vpb.QueryMetadata(name=self.name, id=self.id, relation=self.relation)

    def row_batch(self,
                  cols: List[List[Any]],
                  eow: bool = False,
                  eos: bool = False) -> vpb.RowBatchData:
        assert len(cols) == len(
            self.relation.columns), f"num cols not equal, {len(cols)}, {len(self.relation.columns)}"
        for c in cols[1:]:
            assert len(c) == len(
                cols[0]), f"Rows are not aligned {len(c)}, {len(cols[0])}"
        formatted_cols: List[vpb.Column] = []
        for c, t in zip(cols, self.relation.columns):
            formatted_cols.append(_make_column(c, t.column_type))
        return vpb.RowBatchData(
            table_id=self.id,
            eow=eow,
            eos=eos,
            cols=formatted_cols,
            num_rows=len(cols[0]),
        )

    def metadata_response(self) -> ExecResponse:
        return ExecResponse(data=vpb.ExecuteScriptResponse(status=_ok(), meta_data=self._metadata()))

    def row_batch_response(self, cols: List[List[Any]]) -> ExecResponse:
        # Error out if the rowbatch does not have the right number of columns.
        return ExecResponse(
            data=vpb.ExecuteScriptResponse(
                status=_ok(),
                data=vpb.QueryData(batch=self.row_batch(cols))
            ),
        )

    def end(self) -> ExecResponse:
        """ Sends an end stream message. """
        return ExecResponse(
            data=vpb.ExecuteScriptResponse(
                status=_ok(),
                data=vpb.QueryData(batch=self.row_batch(
                    [[]] * len(self.relation.columns),
                    eos=True,
                    eow=True
                )),
            )
        )


def create_metadata(table_name: str, table_id: str, relation: vpb.Relation) -> vpb.QueryMetadata:
    return vpb.QueryMetadata()


async def iterate_and_pass(table_sub: pxapi.TableSub) -> None:
    """ Processor that iterates over a subscription and does nothing. """
    async for _ in table_sub:
        pass
