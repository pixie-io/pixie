import * as numeral from 'numeral';
import { DataType, UInt128 } from 'types/generated/vizier_pb';

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

export function looksLikeLatencyCol(colName: string, colType: DataType) {
  if (colType !== DataType.FLOAT64) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/latency.*/)) {
    return true;
  }
  return !!colNameLC.match(/p\d{0,2}$/);
}

export function looksLikeCPUCol(colName: string, colType: DataType) {
  if (colType !== DataType.FLOAT64) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/cpu.*/)) {
    return true;
  }
  return !!colNameLC.match(/p\d{0,2}$/);
}

export function looksLikeAlertCol(colName: string, colType: DataType) {
  if (colType !== DataType.BOOLEAN) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  return !!colNameLC.match(/alert.*/);
}

// Converts UInt128 to UUID formatted string.
export function formatUInt128(val: UInt128): string {
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
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES.
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
      return formatUInt128;
    case DataType.STRING:
    default:
      return (d) => d.toString();
  }
}

const stringSortFunc = (a, b) => a.localeCompare(b);

// We send integers as string to make sure we don't have lossy 64-bit integers.
const intSortFunc = (a, b) => Number(a) - Number(b);

const numberSortFunc = (a, b) => Number(a) - Number(b);

const uint128SortFunc = (a, b) => formatUInt128(a).localeCompare(formatUInt128(b));

const boolSortFunc = (a, b) => {
  if (a === b) {
    return 0;
  }
  return Number(a) ? -1 : 1;
};

const sortFuncForType = (type: DataType) => {
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

export const getDataSortFunc = (type: DataType, ascending: boolean) => {
  const f = sortFuncForType(type);
  return (a: any, b: any) => (ascending ? f(a, b) : -f(a, b));
};
