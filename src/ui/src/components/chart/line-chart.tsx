import * as React from 'react';
import {DiscreteColorLegend, LineSeries, XAxis, XYPlot} from 'react-vis';

import {GQLDataTable} from '../../../../vizier/services/api/controller/schema/schema';
import {
    ChartProps, LineSeriesLegends, paletteColorByIndex, TimeValueAxis, withAutoSizer,
} from './chart';
import {extractData} from './data';

interface Point {
  x: number | Date;
  y: number | bigint;
}

interface LineSeries {
  name: string;
  data: Point[];
}

const SUPPORTED_TYPES = new Set(['INT64', 'FLOAT64', 'TIME64NS']);

export function parseData(table: GQLDataTable): LineSeries[] {
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
