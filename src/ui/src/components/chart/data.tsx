import {formatUInt128} from 'utils/format-data';

import {
    GQLDataTable, GQLQueryResult,
} from '../../../../vizier/services/api/controller/schema/schema';

// TODO(malthus): Figure out how to use the enum definition in
// '../../../vizier/services/api/controller/schema/schema';
export function extractData(type: string, col): any[] {
  switch (type) {
    case 'STRING':
      return col.stringData.data;
    case 'TIME64NS':
      return col.time64nsData.data.map((d) => new Date(parseFloat(d) / 1000000));
    case 'INT64':
      // TODO(malthus): D3 doesn't handle bigints, figure out a better solution.
      return col.int64Data.data.map((d) => Number(BigInt(d)));
    case 'FLOAT64':
      return col.float64Data.data.map((d) => parseFloat(d));
    case 'UINT128':
      return col.uint128Data.data.map((d) => formatUInt128(d.high, d.low));
    case 'BOOLEAN':
      return col.booleanData.data;
    default:
      throw (new Error('Unsupported data type: ' + type));
  }
}

// Helper function to extract tables from query results, this is needed due to
// a graphql schema change.
export function tablesFromResults(data: GQLQueryResult): GQLDataTable[] {
  return Array.isArray(data.table) ? data.table : [data.table];
}
