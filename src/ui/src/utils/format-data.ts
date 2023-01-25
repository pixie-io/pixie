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

import numeral from 'numeral';

import { DataType, SemanticType, UInt128 } from 'app/types/generated/vizierapi_pb';

export function formatInt64Data(val: string): string {
  return numeral(val).format('0,0');
}

export function formatBoolData(val: boolean): string {
  return val ? 'true' : 'false';
}

export function formatFloat64Data(val: number, formatStr = '0[.]00'): string {
  // Numeral.js doesn't actually format NaNs, it ignores them.
  if (Number.isNaN(val)) {
    return 'NaN';
  }

  let num = numeral(val).format(formatStr);
  // Numeral.js doesn't have a catch for abs-value decimals less than 1e-6.
  if (num === 'NaN' && Math.abs(val) < 1e-6) {
    num = formatFloat64Data(0);
  }
  return num;
}

export function looksLikeLatencyCol(colName: string, semType: SemanticType, colType: DataType): boolean {
  if (colType !== DataType.FLOAT64 && colType !== DataType.INT64) {
    return false;
  }
  if (semType !== SemanticType.ST_DURATION_NS) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/latency.*/)) {
    return true;
  }
  return !!colNameLC.match(/p\d{0,2}$/);
}

export function looksLikeCPUCol(colName: string, semType: SemanticType, colType: DataType): boolean {
  if (colType !== DataType.FLOAT64) {
    return false;
  }
  if (semType !== SemanticType.ST_PERCENT) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/cpu.*/)) {
    return true;
  }
  return !!colNameLC.match(/p\d{0,2}$/);
}

export function looksLikeAlertCol(colName: string, colType: DataType): boolean {
  if (colType !== DataType.BOOLEAN) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  return !!colNameLC.match(/alert.*/);
}

export function looksLikePIDCol(colName: string, colType: DataType): boolean {
  return colName.toLowerCase() === 'pid' && colType === DataType.INT64;
}

// Converts UInt128PB to UUID formatted string.
export function formatUInt128Protobuf(val: UInt128): string {
  // TODO(zasgar/michelle): Revisit this to check and make sure endianness is correct.
  // Each segment of the UUID is a hex value of 16 nibbles.
  // Note: BigInt support only available in Chrome > 67, FF > 68.
  const hexStrHigh = BigInt(val.getHigh()).toString(16).padStart(16, '0');
  const hexStrLow = BigInt(val.getLow()).toString(16).padStart(16, '0');

  // Sample UUID: 123e4567-e89b-12d3-a456-426655440000.
  // Format is 8-4-4-4-12.
  let uuidStr = '';
  uuidStr += hexStrHigh.substr(0, 8);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(8, 4);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(12, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(0, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(4);
  return uuidStr;
}

export function getDataRenderer(type: DataType): (any) => string {
  // PX_CARNOT_UPDATE_FOR_NEW_TYPES.
  switch (type) {
    case DataType.FLOAT64:
      return formatFloat64Data;
    case DataType.TIME64NS:
      return (d) => new Date(d).toLocaleString();
    case DataType.INT64:
      return formatInt64Data;
    case DataType.BOOLEAN:
      return formatBoolData;
    case DataType.UINT128:
    case DataType.STRING:
    default:
      // Note: UINT128 protobufs are parsed when the result table is loaded, so Uint128 values
      // are strings rather than protos by the time they get to here. As a result, we don't need
      // to call formatUInt128Protobuf here.
      return (d) => d.toString();
  }
}

const stringSortFunc = (a: string, b: string) => a.localeCompare(b);

// We send integers as string to make sure we don't have lossy 64-bit integers.
const intSortFunc = (a, b) => Number(a) - Number(b);

const numberSortFunc = (a, b) => Number(a) - Number(b);

const uint128SortFunc = stringSortFunc;

const boolSortFunc = (a, b) => {
  if (a === b) {
    return 0;
  }
  return Number(a) ? -1 : 1;
};

export const getDataSortFunc = (type: DataType): ((a: unknown, b: unknown) => number) => {
  switch (type) {
    case DataType.FLOAT64:
      return numberSortFunc;
    case DataType.TIME64NS:
      return intSortFunc;
    case DataType.INT64:
      return intSortFunc;
    case DataType.BOOLEAN:
      return boolSortFunc;
    case DataType.UINT128:
      return uint128SortFunc;
    case DataType.STRING:
    default:
      return stringSortFunc;
  }
};
