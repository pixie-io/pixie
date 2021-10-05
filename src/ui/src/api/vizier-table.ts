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

import { Relation, RowBatchData, SemanticType } from 'app/types/generated/vizierapi_pb';
import { parseRows } from 'app/utils/result-data-parsers';
import { dataFromProto } from 'app/utils/result-data-utils';

/**
 * Utility class to handle table data from ExecuteScriptResponse.
 *
 * Converts response data to a format the UI can render quickly, and efficiently processes updates.
 */
export class VizierTable {
  readonly rows: any[] = [];

  readonly batches: RowBatchData[] = [];

  private readonly semanticTypeMap: Map<string, SemanticType>;

  constructor(
    readonly id: string,
    readonly name: string,
    readonly relation: Relation,
    batches: RowBatchData[] = [],
  ) {
    this.semanticTypeMap = relation.getColumnsList().reduce(
      (map, col) => map.set(col.getColumnName(), col.getColumnSemanticType()),
      new Map());
    for (const batch of batches) this.appendBatch(batch);
  }

  appendBatch(batch: RowBatchData): void {
    if (batch.getTableId() !== this.id) {
      throw new Error(`Batch update for table id "${batch.getTableId()}" does not match target table "${this.id}"`);
    }

    this.batches.push(batch);
    const newRows = parseRows(this.semanticTypeMap, dataFromProto(this.relation, [batch]));

    // Fast concatenation: while `a.push(...b)` is clear and concise, it also repeats work. Telling the array ahead of
    // time how much to grow, then assigning items directly to their new locations, involves fewer memory operations.
    const offset = this.rows.length;
    this.rows.length += batch.getNumRows();
    for (let r = 0; r < newRows.length; r++) this.rows[r + offset] = newRows[r];
  }
}
