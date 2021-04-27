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

from src.api.proto.vizierpb import vizierapi_pb2 as vpb


class PxLError(Exception):
    pass


def _format_line(line_num: int, line: str, column: int, message: str) -> str:
    TAB = "  "
    if column > len(line):
        return "\n".join([line, message])
    column_pointer = len(line) * [" "]
    column_pointer[column - 1] = "^"
    out = [
        f"PxL, line {line_num}, {message}",
        line,
        "".join(column_pointer),
    ]
    return TAB + ("\n" + TAB + TAB).join(out)


def _line_col_exception(query: str, error_details: vpb.ErrorDetails, cluster_id: str) -> PxLError:
    query_lines = query.splitlines()
    combined_message = [""]

    for detail in error_details:
        if not detail.compiler_error:
            continue
        err = detail.compiler_error
        combined_message.append(_format_line(
            err.line,
            query_lines[err.line - 1],
            err.column,
            err.message
        ))

    return PxLError(f"cluster: {cluster_id}\n".join(combined_message))


def build_pxl_exception(query: str, err: vpb.Status, cluster_id: str) -> Exception:
    if not err.error_details:
        return ValueError(f"On {cluster_id} {err.message}")
    return _line_col_exception(query, err.error_details, cluster_id)
