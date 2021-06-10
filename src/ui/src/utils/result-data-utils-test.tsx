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

import {
  Column, Float64Column, Int64Column, Relation, RowBatchData, Time64NSColumn,
} from 'app/types/generated/vizierapi_pb';
import { milliToNanoSeconds } from 'app/utils/time';

import * as ResultDataUtils from './result-data-utils';

describe('ResultsToCsv test', () => {
  it('should correctly format results', () => {
    const results = '{"relation":{"columns":[{"columnName":"time_","columnType":"TIME64NS"},'
      + '{"columnName":"http_request","columnType":"STRING"}]},'
      + '"rowBatches": [{"cols":[{"time64nsData":{"data":["1","2","3","4"]}},'
      + '{"stringData":{"data":["a","b","c","d"]}}],"numRows":"4"},'
      + '{"cols":[{"time64nsData":{"data":["5","6","7","8"]}},'
      + '{"stringData":{"data":["{\\"req_id\\": \\"123\\", \\"req_id2\\": \\"{456}\\"}","f","g","h"]}}],'
      + '"numRows":"4"}]}';

    expect(ResultDataUtils.ResultsToCsv(results)).toEqual(
      'time_,http_request\n"1","a"\n"2","b"\n"3","c"\n"4","d"\n"5",'
      + '"""{\\\\""req_id\\\\"": \\\\""123\\\\"", \\\\""req_id2\\\\"": \\\\""{456}\\\\""}"""'
      + '\n"6","f"\n"7","g"\n"8","h"\n',
    );
  });
});

describe('dataFromProto', () => {
  const expected = [
    {
      time: 1234567890,
      float64: 1.111,
      int64: 4567,
    },
    {
      time: 2345678901,
      float64: 2.222,
      int64: 1234,
    },
    {
      time: 3456789012,
      float64: 3.333,
      int64: 9876,
    },
  ];

  const relationCols = [
    new Relation.ColumnInfo(),
    new Relation.ColumnInfo(),
    new Relation.ColumnInfo(),
  ];
  relationCols[0].setColumnName('time');
  relationCols[1].setColumnName('float64');
  relationCols[2].setColumnName('int64');
  const relationsProto = new Relation();
  relationsProto.setColumnsList(relationCols);

  const dataCols1 = [
    new Column(),
    new Column(),
    new Column(),
  ];
  const timeCol1 = new Time64NSColumn();
  timeCol1.setDataList([milliToNanoSeconds(expected[0].time), milliToNanoSeconds(expected[1].time)]);
  dataCols1[0].setTime64nsData(timeCol1);
  const floatCol1 = new Float64Column();
  floatCol1.setDataList([expected[0].float64, expected[1].float64]);
  dataCols1[1].setFloat64Data(floatCol1);
  const int64Col1 = new Int64Column();
  int64Col1.setDataList([expected[0].int64, expected[1].int64]);
  dataCols1[2].setInt64Data(int64Col1);
  const rowBatch1 = new RowBatchData();
  rowBatch1.setColsList(dataCols1);
  rowBatch1.setNumRows(2);

  const dataCols2 = [
    new Column(),
    new Column(),
    new Column(),
  ];
  const timeCol2 = new Time64NSColumn();
  timeCol2.setDataList([milliToNanoSeconds(expected[2].time)]);
  dataCols2[0].setTime64nsData(timeCol2);
  const floatCol2 = new Float64Column();
  floatCol2.setDataList([expected[2].float64]);
  dataCols2[1].setFloat64Data(floatCol2);
  const int64Col2 = new Int64Column();
  int64Col2.setDataList([expected[2].int64]);
  dataCols2[2].setInt64Data(int64Col2);
  const rowBatch2 = new RowBatchData();
  rowBatch2.setColsList(dataCols2);
  rowBatch2.setNumRows(1);

  it('handles multiple rowbatches correctly', () => {
    expect(ResultDataUtils.dataFromProto(relationsProto, [rowBatch1, rowBatch2]))
      .toEqual(expected);
  });

  it('handles single rowbatch correctly', () => {
    expect(ResultDataUtils.dataFromProto(relationsProto, [rowBatch1]))
      .toEqual(expected.slice(0, 2));
  });
});
