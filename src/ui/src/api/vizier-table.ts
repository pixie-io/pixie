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
 * Pressure release valve: to avoid locking up the browser, limit how many rows can be remembered at one time.
 * When appendBatch would add enough rows to exceed this limit, the oldest (based on order received) are pruned to make
 * room. This can be visually weird if the user is sorting by a column other than `time_`. Better than freezing though!
 *
 * Note: Do not set this below 10,000. That is the maximum size of a data window, and pruning below that limit is buggy.
 *
 * This number wasn't chosen scientifically: it's roughly where a 2020 Macbook Pro starts to struggle when it's also
 * busy running an IDE, a build, two browsers, and a dev server. Ideally, the limit would by dynamic based on load.
 */
export const ROW_RETENTION_LIMIT = 50_000;

/**
 * Utility class to handle table data from ExecuteScriptResponse.
 *
 * Converts response data to a format the UI can render quickly, and efficiently processes updates.
 */
export class VizierTable {
  readonly rows: any[] = [];

  readonly batches: RowBatchData[] = [];

  /** Keeps track of the maximum p99 value in each column that contains quantile data. */
  readonly maxQuantiles = new Map<string, number>();

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

    if (this.batches.includes(batch)) {
      throw new Error(`Batch update with ${batch.getNumRows()} rows was appended twice (same object in memory)!`);
    }

    this.batches.push(batch);
    const newRows = parseRows(this.semanticTypeMap, dataFromProto(this.relation, [batch]));

    // Pruning: if the number of rows would put too much pressure on memory, forget the oldest ones to make room first.
    // NOTE: This does not handle the scenario where newRows.length > ROW_RETENTION_LIMIT. Don't set the limit too low.
    const newLength = this.rows.length + newRows.length;
    if (newLength > ROW_RETENTION_LIMIT) {
      this.fastShift(newLength - ROW_RETENTION_LIMIT);
      // It's possible that we prune the current max value for a column. However, it's ALSO possible that the max was
      // a tie with a row that we did not remove. To reconcile this, recompute entirely. Unfortunately, this is slow.
      this.maxQuantiles.clear();
      this.updateMaxQuantiles(this.rows);
    }

    // Update any cumulative values with the new rows, before appending them to the existing rows.
    this.updateMaxQuantiles(newRows);

    // Fast concatenation: while `a.push(...b)` is clear and concise, it also repeats work. Telling the array ahead of
    // time how much to grow, then assigning items directly to their new locations, involves fewer memory operations.
    const offset = this.rows.length;
    this.rows.length += batch.getNumRows();
    for (let r = 0; r < newRows.length; r++) this.rows[r + offset] = newRows[r];
  }

  private updateMaxQuantiles(newRows: any[]) {
    const quantileColumns = [...this.semanticTypeMap.keys()].filter((k) => [
      SemanticType.ST_QUANTILES,
      SemanticType.ST_DURATION_NS_QUANTILES,
    ].includes(this.semanticTypeMap.get(k)));

    for (let r = 0; r < newRows.length; r++) {
      for (const key of quantileColumns) {
        const max = Math.max(
          this.maxQuantiles.get(key) ?? Number.NEGATIVE_INFINITY,
          newRows[r][key]?.p99 ?? Number.NEGATIVE_INFINITY,
        );
        if (max > Number.NEGATIVE_INFINITY) this.maxQuantiles.set(key, max);
      }
    }
  }

  /**
   * Like Array.prototype.shift(), except it removes many elements at a time. Using splice() can do this too, but that
   * creates a new array containing the removed elements. If you don't need that, this method is faster.
   */
  private fastShift(count: number) {
    if (count > this.rows.length) {
      throw new Error(`Tried to prune ${count} rows; there are only ${this.rows.length}!`);
    }

    if (count === this.rows.length) { // Shortcut. Also, the logic below breaks in this case anyway.
      this.rows.length = 0;
      return;
    }

    // Like with appendBatch above, repeated shift() takes more memory operations than this more direct approach.
    for (let i = count; i < this.rows.length; i++) {
      this.rows[i - count] = this.rows[i];
    }
    this.rows.length -= count;
  }
}
