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
  Column,
  BooleanColumn,
  StringColumn,
  DataType,
  Relation,
  RowBatchData,
  SemanticType,
} from 'app/types/generated/vizierapi_pb';
import { VizierTable } from './vizier-table';

describe('VizierTable', () => {
  const id = 'foo';
  const name = 'Foo';

  const relation = new Relation().setColumnsList([
    new Relation.ColumnInfo()
      .setColumnName('one')
      .setColumnType(DataType.BOOLEAN),
    new Relation.ColumnInfo()
      .setColumnName('two')
      .setColumnType(DataType.STRING)
      .setColumnSemanticType(SemanticType.ST_QUANTILES),
  ]);

  const inBatches: RowBatchData[] = [
    new RowBatchData()
      .setTableId(id)
      .setColsList([
        new Column().setBooleanData(new BooleanColumn().setDataList([
          true, true, true, true, true,
        ])),
        new Column().setStringData(new StringColumn().setDataList(
          Array(5).fill(null).map((_, i) => JSON.stringify({ p50: i * 5, p90: i * 5 + 1, p99: i * 5 + 2 })),
        )),
      ]),
    new RowBatchData()
      .setTableId(id)
      .setColsList([
        new Column().setBooleanData(new BooleanColumn().setDataList([false])),
        new Column().setStringData(new StringColumn().setDataList([JSON.stringify({ p50: 25, p90: 26, p99: 27 })])),
      ]),
  ];

  it('Constructs without seed rows', () => {
    const table = new VizierTable(id, name, Relation.deserializeBinary(relation.serializeBinary()));
    expect(table.batches.length).toBe(0);
    expect(table.rows.length).toBe(0);
  });

  it('Constructs with seed rows', () => {
    const table = new VizierTable(
      id,
      name,
      Relation.deserializeBinary(relation.serializeBinary()),
      inBatches.map((batch) => RowBatchData.deserializeBinary(batch.serializeBinary())),
    );
    expect(table.batches.length).toBe(2);
    expect(table.rows.length).toBe(6);
    // Testing the final output is technically redundant with dataFromProto and parseRows tests, but it's easy to do.
    expect(table.rows).toEqual([
      { one: true, two: { p50: 0, p90: 1, p99: 2 } },
      { one: true, two: { p50: 5, p90: 6, p99: 7 } },
      { one: true, two: { p50: 10, p90: 11, p99: 12 } },
      { one: true, two: { p50: 15, p90: 16, p99: 17 } },
      { one: true, two: { p50: 20, p90: 21, p99: 22 } },
      { one: false, two: { p50: 25, p90: 26, p99: 27 } },
    ]);
  });

  it('Appends batches after construction', () => {
    const table = new VizierTable(id, name, Relation.deserializeBinary(relation.serializeBinary()));

    table.appendBatch(RowBatchData.deserializeBinary(inBatches[0].serializeBinary()));
    expect(table.batches.length).toBe(1);
    expect(table.rows.length).toBe(5);
    expect(table.rows).toEqual([
      { one: true, two: { p50: 0, p90: 1, p99: 2 } },
      { one: true, two: { p50: 5, p90: 6, p99: 7 } },
      { one: true, two: { p50: 10, p90: 11, p99: 12 } },
      { one: true, two: { p50: 15, p90: 16, p99: 17 } },
      { one: true, two: { p50: 20, p90: 21, p99: 22 } },
    ]);

    table.appendBatch(RowBatchData.deserializeBinary(inBatches[1].serializeBinary()));
    expect(table.batches.length).toBe(2);
    expect(table.rows.length).toBe(6);
    expect(table.rows).toEqual([
      { one: true, two: { p50: 0, p90: 1, p99: 2 } },
      { one: true, two: { p50: 5, p90: 6, p99: 7 } },
      { one: true, two: { p50: 10, p90: 11, p99: 12 } },
      { one: true, two: { p50: 15, p90: 16, p99: 17 } },
      { one: true, two: { p50: 20, p90: 21, p99: 22 } },
      { one: false, two: { p50: 25, p90: 26, p99: 27 } },
    ]);
  });

  it('Keeps track of the highest p99 in each quantile column', () => {
    const table = new VizierTable(id, name, Relation.deserializeBinary(relation.serializeBinary()));

    table.appendBatch(RowBatchData.deserializeBinary(inBatches[0].serializeBinary()));
    expect([...table.maxQuantiles.entries()]).toEqual([['two', 22]]);

    table.appendBatch(RowBatchData.deserializeBinary(inBatches[1].serializeBinary()));
    expect([...table.maxQuantiles.entries()]).toEqual([['two', 27]]);
  });
});
