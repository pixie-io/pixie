import * as React from 'react';
import {DiscreteColorLegend, LineSeries, XAxis, XYPlot} from 'react-vis';

import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';
import {ChartProps, LineSeriesLegends, paletteColorByIndex, TimeValueAxis} from './chart';
import {parseLineData} from './data';

interface Point {
  x: number | Date;
  y: number | bigint;
}

interface LineSeries {
  name: string;
  data: Point[];
}

const SUPPORTED_TYPES = new Set(['INT64', 'FLOAT64', 'TIME64NS']);

export function parseData(data: GQLQueryResult): LineSeries[] {
  try {
    const tables = [];
    if (Array.isArray(data.table)) {
      tables.push(...data.table);
    } else {
      tables.push(data.table);
    }
    return tables.reduce((lines, table) => [...lines, ...parseLineData(table)], []);
  } catch (e) {
    return [];
  }
}

export const LineChart: React.FC<ChartProps> = ({ data, height, width }) => {
  const lines = parseData(data);
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
};
