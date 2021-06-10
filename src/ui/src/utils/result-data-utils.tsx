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

import * as _ from 'lodash';
import {
  Column, Relation, RowBatchData,
} from 'app/types/generated/vizierapi_pb';
import { formatUInt128Protobuf } from './format-data';
import { nanoToMilliSeconds } from './time';

export function ResultsToCsv(results: string): string {
  const jsonResults = JSON.parse(results);
  let csvStr = '';

  csvStr += `${_.map(jsonResults.relation.columns, 'columnName').join()}\n`;
  _.each(jsonResults.rowBatches, (rowBatch) => {
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
  });

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
    return column.getStringData().getDataList();
  } if (column.hasTime64nsData()) {
    const data = column.getTime64nsData().getDataList();
    return data.map(nanoToMilliSeconds);
  }
  throw (new Error(`Unsupported data type: ${column.getColDataCase()}`));
}

export function dataFromProto(
  relation: Relation,
  data: RowBatchData[],
): any[] {
  const results = [];

  const colRelations = relation.getColumnsList();

  data.forEach((batch) => {
    const rows = [];
    for (let i = 0; i < batch.getNumRows(); i++) {
      rows.push({});
    }
    const cols = batch.getColsList();
    cols.forEach((col, i) => {
      const name = colRelations[i].getColumnName();
      columnFromProto(col).forEach((d, j) => {
        rows[j][name] = d;
      });
    });
    results.push(...rows);
  });
  return results;
}
