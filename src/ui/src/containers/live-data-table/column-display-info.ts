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

import { SelectedPercentile } from 'app/components';
import { DataType, Relation, SemanticType } from 'app/types/generated/vizierapi_pb';

export interface QuantilesDisplayState {
  selectedPercentile: SelectedPercentile;
}

type TypeSpecificDisplayState = QuantilesDisplayState | Record<string, unknown>;

function defaultDisplayState(st: SemanticType) {
  if (st === SemanticType.ST_QUANTILES || st === SemanticType.ST_DURATION_NS_QUANTILES) {
    return { selectedPercentile: 'p99' };
  }
  return {};
}

export interface ColumnDisplayInfo {
  columnName: string;
  baseTitle: string;
  type: DataType;
  semanticType: SemanticType;
  displayState: TypeSpecificDisplayState;
}

export function displayInfoFromColumn(col: Relation.ColumnInfo): ColumnDisplayInfo {
  const st = col.getColumnSemanticType();
  return {
    columnName: col.getColumnName(),
    baseTitle: col.getColumnName(),
    type: col.getColumnType(),
    semanticType: st,
    displayState: defaultDisplayState(st),
  };
}

export function titleFromInfo(col: ColumnDisplayInfo): string {
  if (col.semanticType === SemanticType.ST_QUANTILES || col.semanticType === SemanticType.ST_DURATION_NS_QUANTILES) {
    const quantilesState = col.displayState as QuantilesDisplayState;
    const selectedPercentile = quantilesState.selectedPercentile || 'p99';
    return `${col.baseTitle} (${selectedPercentile})`;
  }
  return col.baseTitle;
}
