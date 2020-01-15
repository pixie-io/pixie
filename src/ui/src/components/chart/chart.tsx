import * as moment from 'moment';
import * as numeral from 'numeral';
import * as React from 'react';
import {AutoSizer} from 'react-virtualized';
import {DiscreteColorLegend, XAxis, YAxis} from 'react-vis';

import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';

export interface ChartProps {
  data: GQLQueryResult;
}

export interface LineSeriesData {
  name: string;
  data: LineSeriesPoint[];
}

export interface LineSeriesPoint {
  x: number | Date;
  y: number | bigint;
}

const CHART_COLORS = [
  '#4385f4',  // blue
  '#56ccf2',  // cyan
  '#eb5757',  // red
  '#6fcf97',  // green
  '#ffa2a2',  // not so pink
];

export function paletteColorByIndex(index: number): string {
  return CHART_COLORS[index % CHART_COLORS.length];
}

interface LineSeriesLegendProps {
  lines: LineSeriesData[];
  stroke?: any;
  style?: any;
}

export const LineSeriesLegends: React.FC<LineSeriesLegendProps> = ({ lines, stroke = {}, style = {} }) => {
  if (lines.length === 0) {
    return null;
  }
  const legends = lines.map((lineData, i) => ({
    title: lineData.name,
    color: paletteColorByIndex(i),
    ...stroke,
  }));
  return <DiscreteColorLegend orientation='horizontal' items={legends} style={style} />;
};

/**
 * X and Y axis for the chart. This is not a React component as the axes have
 * to be children of the XYPlot. Usage:
 * <XYPlot>
 *   {...TimeValueAxis()}
 * </XYPlot>
 */
export function TimeValueAxis() {
  return [
    <XAxis tickFormat={(value) => moment(value).format('hh:mm:ss')} />,
    <YAxis tickFormat={(value) => numeral(value).format('0.[0]a')} />,
  ];
}

interface AutoSizerProps {
  width: number;
  height: number;
  [prop: string]: any;
}

export function withAutoSizer(WrappedComponent: React.ComponentType<AutoSizerProps>) {
  return (props) => (
    <AutoSizer>
      {({ height, width }) => (
        <WrappedComponent width={width} height={height} {...props} />
      )}
    </AutoSizer>
  );
}
