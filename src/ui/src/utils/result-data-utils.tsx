/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as jspb from 'google-protobuf';

import {
  Column, Relation, RowBatchData,
} from 'app/types/generated/vizierapi_pb';

import { formatUInt128Protobuf } from './format-data';
import { u8aToStr } from './result-data-parsers';
import { nanoToMilliSeconds } from './time';

export function ResultsToCsv(results: string): string {
  const jsonResults = JSON.parse(results);
  let csvStr = '';

  const colNames = jsonResults.relation.columns.map((column) => (column.columnName));
  csvStr += `${colNames.join()}\n`;
  for (const rowBatch of jsonResults.rowBatches) {
    const numRows = parseInt(rowBatch.numRows, 10);
    const numCols = rowBatch.cols.length;
    for (let i = 0; i < numRows; i++) {
      const rowData = [];
      for (let j = 0; j < numCols; j++) {
        const colKey = Object.keys(rowBatch.cols[j])[0];
        let data = rowBatch.cols[j][colKey].data[i];
        if (typeof data === 'string') {
          data = data.replace(/"/g, '\\\\""');
          data = data.replace(/^{/g, '""{');
          data = data.replace(/}$/g, '}""');
          data = data.replace(/^\[/g, '""[');
          data = data.replace(/\[$/g, ']""');
        }
        rowData.push(`"${data}"`);
      }
      csvStr += `${rowData.join()}\n`;
    }
  }

  return csvStr;
}

export function columnFromProto(column: Column): (number | boolean | string)[] {
  if (column.hasBooleanData()) {
    return column.getBooleanData().getDataList();
  } if (column.hasInt64Data()) {
    return column.getInt64Data().getDataList();
  } if (column.hasUint128Data()) {
    return column.getUint128Data().getDataList().map((uint128) => formatUInt128Protobuf(uint128));
  } if (column.hasFloat64Data()) {
    return column.getFloat64Data().getDataList();
  } if (column.hasStringData()) {
    // getDataList() returns a {!Array<string>|!Array<!Uint8Array>} but we want to coerce into a single
    // type {!Array<Uint8Array?>} so we wrap it in bytesListAsU8 before parsing into string.
    return jspb.Message.bytesListAsU8(column.getStringData().getDataList()).map((u8a) => u8aToStr(u8a));
  } if (column.hasTime64nsData()) {
    const data = column.getTime64nsData().getDataList();
    return data.map(nanoToMilliSeconds);
  }
  throw (new Error(`Unsupported data type: ${column.getColDataCase()}`));
}

export function dataFromProto(
  relation: Relation,
  batches: RowBatchData[],
): any[] {
  // There can be dozens of batches with thousands of rows each. As such, this method tries to be somewhat fast:
  // - Pre-allocating the results array (cheaper than pushing rows one-at-a-time or in batches)
  // - Populating each row of the array at the last possible moment, to avoid an extra loop
  // - Looping without iterators (for..of, for..in) or callbacks (forEach)
  // These optimizations are a bit clunky to read, but this method is costly and in the critical result processing path.

  const total = batches.reduce((sum, batch) => sum + batch.getNumRows(), 0);
  const results = Array(total); // Not filling/mapping at this stage; it's slightly faster to create objects as needed.
  let offset = 0;

  const colRelations = relation.getColumnsList();

  for (const batch of batches) {
    const cols = batch.getColsList();
    for (let i = 0; i < cols.length; i++) {
      const name = colRelations[i].getColumnName();
      const colData = columnFromProto(cols[i]);
      for (let j = 0; j < colData.length; j++) {
        if (results[j + offset] == null) results[j + offset] = {};
        results[j + offset][name] = colData[j];
      }
    }
    offset += batch.getNumRows();
  }
  return results;
}
