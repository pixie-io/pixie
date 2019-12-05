import {formatUInt128} from 'utils/format-data';

import {
    GQLDataTable, GQLQueryResult,
} from '../../../../vizier/services/api/controller/schema/schema';
import {LineSeriesData} from './chart';

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

const SUPPORTED_TYPES = new Set(['INT64', 'FLOAT64', 'TIME64NS']);

export function parseLineData(table: GQLDataTable): LineSeriesData[] {
  try {
    let timeColName = '';
    const relation = table.relation;
    const columns = new Map<string, any[]>();
    relation.colNames.forEach((name, i) => {
      const type = relation.colTypes[i];
      if (!SUPPORTED_TYPES.has(type)) {
        return;
      }
      if (type === 'TIME64NS') {
        timeColName = name;
      }
      columns.set(name, []);
    });
    if (!timeColName) {
      return [];
    }
    const { rowBatches } = JSON.parse(table.data);
    for (const batch of rowBatches) {
      batch.cols.forEach((col, i) => {
        const name = relation.colNames[i];
        if (!columns.has(name)) {
          return;
        }
        const type = relation.colTypes[i];
        columns.get(name).push(...extractData(type, col));
      });
    }
    const timestamps = columns.get(timeColName);
    const timeseries = [];
    for (const [name, col] of columns) {
      if (name === timeColName) {
        continue;
      }
      const points = col.map((d, i) => ({ x: timestamps[i], y: Number(d) }));
      points.sort((a, b) => b.x - a.x);
      timeseries.push({ data: points, name });
    }
    return timeseries;
  } catch (e) {
    return [];
  }
}
