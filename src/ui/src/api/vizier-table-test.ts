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
  SemanticType, Int64Column,
} from 'app/types/generated/vizierapi_pb';
import { strToU8a } from 'app/utils/result-data-parsers';

import { ROW_RETENTION_LIMIT, VizierTable } from './vizier-table';

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
          Array(5).fill(null).map((_, i) => strToU8a(JSON.stringify({ p50: i * 5, p90: i * 5 + 1, p99: i * 5 + 2 }))),
        )),
      ]),
    new RowBatchData()
      .setTableId(id)
      .setColsList([
        new Column().setBooleanData(new BooleanColumn().setDataList([false])),
        new Column().setStringData(new StringColumn().setDataList([
          strToU8a(JSON.stringify({ p50: 25, p90: 26, p99: 27 })),
        ])),
      ]),
  ];

  const hugeBatchRelation = new Relation().setColumnsList([
    new Relation.ColumnInfo().setColumnName('int').setColumnType(DataType.INT64),
  ]);
  const hugeBatch = new RowBatchData().setTableId(id).setColsList([
    new Column().setInt64Data(new Int64Column().setDataList(
      Array(ROW_RETENTION_LIMIT - 1).fill(null).map((_, i) => i),
    )),
  ]);

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

  it('Sheds excessive rows when there are too many to keep in memory', () => {
    const table = new VizierTable(id, name, Relation.deserializeBinary(hugeBatchRelation.serializeBinary()));
    // Start with one row fewer than the limit
    table.appendBatch(RowBatchData.deserializeBinary(hugeBatch.serializeBinary()));
    expect(table.rows.length).toBe(ROW_RETENTION_LIMIT - 1);

    // Add that many rows again, which results in ((limit * 2) - 2) rows.
    table.appendBatch(RowBatchData.deserializeBinary(hugeBatch.serializeBinary()));

    // The table should remove (limit - 2) rows from the top to make room. This means that the last item from
    // the original batch should still be there, as well as the entire set of added rows.
    expect(table.rows.length).toBe(ROW_RETENTION_LIMIT);
    expect(table.rows[0].int).toBe(ROW_RETENTION_LIMIT - 2);
    expect(table.rows[1].int).toBe(0);
  });

  it('Updates highest p99 correctly when shedding excessive rows', () => {
    const table = new VizierTable(id, name, new Relation().setColumnsList([
      new Relation.ColumnInfo()
        .setColumnName('quantile')
        .setColumnType(DataType.STRING)
        .setColumnSemanticType(SemanticType.ST_QUANTILES),
    ]));

    table.appendBatch(
      new RowBatchData()
        .setTableId(id)
        .setColsList([
          new Column().setStringData(new StringColumn().setDataList(
            Array(ROW_RETENTION_LIMIT - 1).fill(null)
              .map((_, i) => strToU8a(JSON.stringify({ p50: 0, p90: 0, p99: i * 2 }))),
          )),
        ]),
    );

    expect(table.maxQuantiles.get('quantile')).toBe(ROW_RETENTION_LIMIT * 2 - 4);

    table.appendBatch(
      new RowBatchData()
        .setTableId(id)
        .setColsList([
          new Column().setStringData(new StringColumn().setDataList(
            Array(ROW_RETENTION_LIMIT).fill(null) // Enough to COMPLETELY empty the buffer
              .map((_, i) => strToU8a(JSON.stringify({ p50: 0, p90: 0, p99: i }))),
          )),
        ]),
    );

    // If the old max value falls out, the next max value (which may be smaller) should be what gets kept.
    expect(table.maxQuantiles.get('quantile')).toBe(ROW_RETENTION_LIMIT - 1);
  });

  it('Handles non utf-8 strings', () => {
    const table = new VizierTable(id, name, new Relation().setColumnsList([
      new Relation.ColumnInfo()
        .setColumnName('bytes')
        .setColumnType(DataType.STRING),
    ]));

    table.appendBatch(
      new RowBatchData()
        .setTableId(id)
        .setColsList([
          // 159 is an invalid start byte for utf-8.
          new Column().setStringData(new StringColumn().setDataList([new Uint8Array([159])])),
        ]),
    );
    expect(table.rows).toEqual([
      // Converting bytes to string should encode as the replacement character.
      { bytes: '�' },
    ]);
  });

  // Omitted test case: when a batch comes in that's bigger than ROW_RETENTION_LIMIT (intentionally ignored scenario).
});
