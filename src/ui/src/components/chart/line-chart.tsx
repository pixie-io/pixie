import * as moment from 'moment';
import * as numeral from 'numeral';
import * as React from 'react';
import {HorizontalGridLines, LineSeries, XAxis, XYPlot, YAxis} from 'react-vis';

import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';

interface LineChartProps {
  data: GQLQueryResult;
  height: number;
  width: number;
}

interface Point {
  x: number | Date;
  y: number | bigint;
}

interface LineSeriesData {
  name: string;
  data: Point[];
}

// Formats int64 data, the input type is a string because JS does not
// natively support 64-bit data.
function formatInt64Data(val: string): string {
  return numeral(val).format('0,0');
}

function extractData(type: string, col): any[] {
  switch (type) {
    case 'TIME64NS':
      return col.time64nsData.data.map((d) => new Date(parseFloat(d) / 1000000));
    case 'INT64':
      return col.int64Data.data.map((d) => BigInt(d));
    case 'FLOAT64':
      return col.float64Data.data.map((d) => parseFloat(d));
    default:
      throw (new Error('Unknown data type: ' + type));
  }
}

const SUPPORTED_TYPES = new Set(['INT64', 'FLOAT64', 'TIME64NS']);

function parseData(data: GQLQueryResult): LineSeriesData[] {
  try {
    let timeColName = '';
    const relation = data.table.relation;
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
    const { rowBatches } = JSON.parse(data.table.data);
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
      const points = [];
      for (let i = 1; i < col.length; i++) {
        points.push({ x: timestamps[i], y: Number(col[i] - col[i - 1]) });
      }
      timeseries.push({ data: points, name });
    }
    return timeseries;
  } catch (e) {
    return [];
  }
}

const COLORS = ['red', 'yellow', 'blue'];

export const LineChart: React.FC<LineChartProps> = ({ data, height, width }) => {
  const lines = parseData(data);
  return (
    <XYPlot
      height={height}
      width={width}
    >
      {
        lines.map((line, i) => (
          <LineSeries
            key={line.name}
            data={line.data}
            color={COLORS[i % COLORS.length]}
          />
        ))
      }
      <XAxis tickFormat={(value) => moment(value).format('hh:mm:ss')} />
      <YAxis tickFormat={(value) => numeral(value).format('0.0a')} />
    </XYPlot>
  );
};
