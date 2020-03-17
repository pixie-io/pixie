import {Table} from 'common/vizier-grpc-client';
import * as React from 'react';
import {Highlight, Hint, LineSeries, MarkSeries, XYPlot} from 'react-vis';
import {DataType} from 'types/generated/vizier_pb';
import {columnFromProto} from 'utils/result-data-utils';

import {
    LineSeriesData, LineSeriesLegends, paletteColorByIndex, TimeValueAxis, withAutoSizer,
} from './chart';
import {parseData as parseLineData} from './line-chart';

interface Point {
  x: number | Date;
  y: number | bigint;
  props?: { [key: string]: any };
}

interface ScatterPlotData {
  points: Point[];
  lines?: LineSeriesData[];
}

export function parseData(tables: Table[]): ScatterPlotData | null {
  try {
    let lines = [];
    if (tables.length < 1) {
      return null;
    }
    const scatter = getScatterPoints(tables[0]);
    if (scatter.length === 0) {
      return null;
    }
    if (tables.length > 1) {
      lines = parseLineData(tables[1]);
    }
    return { points: scatter, lines };
  } catch (e) {
    return null;
  }
}

function getScatterPoints(table: Table): Point[] {
  const relation = table.relation;
  if (relation.getColumnsList().length < 2) {
    // There should be at least 2 columns.
    return [];
  }
  if (relation.getColumnsList()[0].getColumnType() !== DataType.TIME64NS ||
    (relation.getColumnsList()[1].getColumnType() !== DataType.INT64 &&
      relation.getColumnsList()[1].getColumnType() !== DataType.FLOAT64)) {
    return [];
  }
  const rowBatches = table.data;
  const out: Point[] = [];
  for (const batch of rowBatches) {
    const cols = batch.getColsList().map(columnFromProto);
    for (let r = 0; r < cols[0].length; r++) {
      const row = { x: cols[0][r], y: cols[1][r] };
      for (let c = 2; c < cols.length; c++) {
        const name = relation.getColumnsList()[c].getColumnName();
        row[name] = cols[c][r];
      }
      out.push(row);
    }
  }
  return out;
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

export const ScatterPlot = React.memo<ScatterPlotData>(withAutoSizer(
  ({ points, lines = [], height, width }) => {
    if (points.length === 0) {
      return null;
    }
    const [value, setValue] = React.useState(null);
    const [brush, setBrush] = React.useState(null);

    const lineSeries = lines.map((lineData, i) => (
      <LineSeries
        key={`line-${i}}`}
        data={lineData.data}
        strokeStyle='dashed'
        color={paletteColorByIndex(i)}
      />
    ));
    return (
      <XYPlot
        style={{ position: 'relative' }}
        width={width}
        height={height}
        onMouseLeave={() => setValue(null)}
        xDomain={brush && [brush.left, brush.right]}
      >
        <MarkSeries
          data={points}
          onNearestXY={(val) => setValue(val)}
        />
        {lineSeries}
        {!!value ? <Hint value={value} format={formatHint} /> : null}
        <Highlight
          enableY={false}
          onBrushEnd={(br) => setBrush(br)}
        />
        {...TimeValueAxis()}
        <LineSeriesLegends
          lines={lines}
          style={{ position: 'absolute', right: 0, top: '-2.5rem' }}
          stroke={{ strokeStyle: 'dashed' }}
        />
      </XYPlot >
    );
  }));
