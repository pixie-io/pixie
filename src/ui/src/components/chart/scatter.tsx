import * as moment from 'moment';
import * as numeral from 'numeral';
import * as React from 'react';
import {Hint, MarkSeries, XAxis, XYPlot, YAxis} from 'react-vis';

import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';
import {ChartProps} from './chart';
import {extractData} from './data';

interface Point {
  x: number | Date;
  y: number | bigint;
  props?: { [key: string]: any };
}

export function parseData(data: GQLQueryResult): Point[] {
  try {
    const relation = data.table.relation;
    if (relation.colNames.length < 2) {
      // There should be at least 2 columns.
      return [];
    }
    if (relation.colTypes[0] !== 'TIME64NS' ||
      (relation.colTypes[1] !== 'INT64' && relation.colTypes[1] !== 'FLOAT64')) {
      return [];
    }
    const { rowBatches } = JSON.parse(data.table.data);
    const out: Point[] = [];
    for (const batch of rowBatches) {
      const cols = batch.cols.map((col, i) => {
        const type = relation.colTypes[i];
        return extractData(type, col);
      });
      for (let r = 0; r < cols[0].length; r++) {
        const row = { x: cols[0][r], y: cols[1][r] };
        for (let c = 2; c < cols.length; c++) {
          const name = relation.colNames[c];
          row[name] = cols[c][r];
        }
        out.push(row);
      }
    }
    return out;
  } catch (e) {
    return [];
  }
}

function formatHint(value: Point) {
  const hints = [];
  for (const key of Object.keys(value)) {
    if (key === 'x') {
      hints.push({ title: 'time', value: value[key].toLocaleString() });
    } else {
      hints.push({ title: key, value: value[key] });
    }
  }
  return hints;
}

export const ScatterPlot: React.FC<ChartProps> = ({ data, height, width }) => {
  const series = parseData(data);
  const [value, setValue] = React.useState(null);

  return (
    <XYPlot
      height={height}
      width={width}
      onMouseLeave={() => setValue(null)}
    >
      <MarkSeries
        data={series}
        onNearestXY={(val) => setValue(val)}
      />
      {!!value ? <Hint value={value} format={formatHint} /> : null}
      <XAxis tickFormat={(val) => moment(val).format('hh:mm:ss')} />
      <YAxis tickFormat={(val) => numeral(val).format('0.0a')} />
    </XYPlot>
  );
};
