import {formatUInt128} from 'utils/format-data';

// TODO(malthus): Figure out how to use the enum definition in
// '../../../vizier/services/api/controller/schema/schema';
export function extractData(type: string, col): any[] {
  switch (type) {
    case 'STRING':
      return col.stringData.data;
    case 'TIME64NS':
      return col.time64nsData.data.map((d) => new Date(parseFloat(d) / 1000000));
    case 'INT64':
      return col.int64Data.data.map((d) => BigInt(d));
    case 'FLOAT64':
      return col.float64Data.data.map((d) => parseFloat(d));
    case 'UINT128':
      return col.uint128Data.data.map((d) => formatUInt128(d.high, d.low));
    default:
      throw (new Error('Unsupported data type: ' + type));
  }
}
