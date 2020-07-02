import * as React from 'react';
import QuantilesBoxWhisker from 'components/quantiles-box-whisker/quantiles-box-whisker';
import { getLatencyLevel, GaugeLevel } from 'components/format-data/latency';
import { Relation } from 'types/generated/vizier_pb';
import { looksLikeLatencyCol } from 'utils/format-data';

// Expects a p99 field in colName.
export function getMaxQuantile(rows: any[], colName: string): number {
  let max;
  for (let i = 0; i < rows.length; ++i) {
    const row = rows[i];
    if (row[colName]) {
      const { p99 } = row[colName];
      if (p99 != null) {
        if (max != null) {
          max = Math.max(max, p99);
        } else {
          max = p99;
        }
      }
    }
  }
  return max;
}

// Expects data to contain p50, p90, and p99 fields.
export function quantilesRenderer(colInfo: Relation.ColumnInfo, rows: any[]) {
  const colName = colInfo.getColumnName();

  const max = getMaxQuantile(rows, colName);
  return function renderer(val) {
    const { p50, p90, p99 } = val;
    let p99Level: GaugeLevel = 'none';
    if (looksLikeLatencyCol(colName, colInfo.getColumnType())) {
      p99Level = getLatencyLevel(p99);
    }
    return <QuantilesBoxWhisker p50={p50} p90={p90} p99={p99} max={max} p99Level={p99Level} />;
  };
}
