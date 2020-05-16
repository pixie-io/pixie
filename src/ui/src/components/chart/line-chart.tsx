import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';
import { LineSeries, XYPlot } from 'react-vis';
import { DataType } from 'types/generated/vizier_pb';
import withAutoSizer from 'utils/autosizer';
import { columnFromProto } from 'utils/result-data-utils';

import { LineSeriesLegends, paletteColorByIndex, TimeValueAxis } from './chart';

interface Point {
  x: number | Date;
  y: number | bigint;
}

interface LineSeries {
  name: string;
  data: Point[];
}

const SUPPORTED_TYPES = new Set<DataType>([DataType.INT64, DataType.FLOAT64, DataType.TIME64NS]);

export function parseData(table: Table): LineSeries[] {
  try {
    let timeColName = '';
    const relation = table.relation.getColumnsList();
    const columns = new Map<string, any[]>();
    relation.forEach((col) => {
      const type = col.getColumnType();
      const name = col.getColumnName();
      if (!SUPPORTED_TYPES.has(type)) {
        return;
      }
      if (type === DataType.TIME64NS) {
        timeColName = name;
      }
      columns.set(name, []);
    });
    if (!timeColName) {
      return [];
    }
    const rowBatches = table.data;
    for (const batch of rowBatches) {
      const cols = batch.getColsList();
      cols.forEach((col, i) => {
        const name = relation[i].getColumnName();
        if (!columns.has(name)) {
          return;
        }
        columns.get(name).push(...columnFromProto(col));
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

interface LineChartProps {
  lines: LineSeries[];
}

export const LineChart = React.memo<LineChartProps>(withAutoSizer(
  ({ lines, height, width }) => {
    if (lines.length < 1) {
      return null;
    }
    return (
      <XYPlot
        height={height}
        width={width}
        style={{ position: 'relative' }}
      >
        {
          lines.map((line, i) => (
            <LineSeries
              key={line.name}
              data={line.data}
              color={paletteColorByIndex(i)}
            />
          ))
        }
        <LineSeriesLegends
          lines={lines}
          style={{ position: 'absolute', right: 0, top: '-2.5rem' }}
        />
        {...TimeValueAxis()}
      </XYPlot>
    );
  }));
