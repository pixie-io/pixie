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

import { SemanticType } from 'app/types/generated/vizierapi_pb';
import { checkExhaustive } from 'app/utils/check-exhaustive';
import { getDataSortFunc } from 'app/utils/format-data';

import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

// Sort funcs for semantic column types (when the pure data type alone doesn't produce
// the sort result that we want.

/**
 * Returns a sorting function that extracts and compares a specific field from the compared objects.
 * Sorts missing (null, undefined) values to the bottom when in ascending order.
 *
 * @param fieldName What field to look at when comparing two objects that both may have that field.
 */
export function fieldSortFunc(fieldName: string) {
  return (a: Record<string, unknown>, b: Record<string, unknown>): number => {
    const aVal = a?.[fieldName];
    const bVal = b?.[fieldName];

    // Number(true) returns 1, Number(false) returns 0.
    const aNull = Number(aVal == null);
    const bNull = Number(bVal == null);

    // Nulls come last.
    if (aNull || bNull) {
      return aNull - bNull;
    }

    // The type must be in a const, rather than directly referenced, for checkExhaustive
    // and switch(typeof aVal) to realize they're referring to the same (narrowed) type.
    const aType = typeof aVal;
    const bType = typeof bVal;
    if (aType !== bType) {
      throw new Error(`Cannot compare disparate types '${aType}' and '${bType}' (a=${aVal}; b=${bVal})`);
    }

    let result: number;
    switch (aType) {
      case 'number':
      case 'bigint':
      case 'boolean':
        // Number(true) is 1, Number(false) and Number(null) are 0.
        result = Number(aVal) - Number(bVal);
        break;
      case 'string':
        result = (aVal as string).localeCompare(bVal as string);
        break;
      case 'object':
      case 'function':
      case 'symbol':
      case 'undefined':
        throw new Error(`Cannot compare values of type '${aType}'`);
      default:
        checkExhaustive(aType);
    }

    return result;
  };
}

/**
 * Service names can be both plain strings and JSON-encoded string arrays.
 * This sorts by the first item (or plain value), then the second if there's a tie, and so on.
 */
export function serviceSortFunc(): (a: string, b: string) => number {
  // Instead of parsing JSON arrays (expensive), we can do a bit of string manipulation for the same result.
  // First, flatten '["foo","bar"]' into 'foo,bar'; then swap the commas for a tab character (earliest printable char).
  // Thus, a natural order would be: 'a', 'a,b', 'b'.
  const flattenStringArrayRe = new RegExp('["\\[\\]]', 'g');
  const commaRe = new RegExp(',', 'g');
  return (a, b) => {
    const aCmp = a.replace(flattenStringArrayRe, '').replace(commaRe, '\t');
    const bCmp = b.replace(flattenStringArrayRe, '').replace(commaRe, '\t');
    return aCmp.localeCompare(bCmp);
  };
}

export function getSortFunc(display: ColumnDisplayInfo): (a: unknown, b: unknown) => number {
  let f;
  switch (display.semanticType) {
    case SemanticType.ST_QUANTILES:
    case SemanticType.ST_DURATION_NS_QUANTILES: {
      const quantilesDisplay = display.displayState as QuantilesDisplayState;
      const selectedPercentile = quantilesDisplay.selectedPercentile || 'p99';
      f = fieldSortFunc(selectedPercentile);
      break;
    }
    case SemanticType.ST_POD_STATUS:
      f = fieldSortFunc('phase');
      break;
    case SemanticType.ST_CONTAINER_STATUS:
      f = fieldSortFunc('state');
      break;
    case SemanticType.ST_SCRIPT_REFERENCE:
      f = fieldSortFunc('label');
      break;
    case SemanticType.ST_SERVICE_NAME:
      f = serviceSortFunc();
      break;
    default: {
      f = getDataSortFunc(display.type);
      break;
    }
  }
  return (a, b) => f(a[display.columnName], b[display.columnName]);
}
