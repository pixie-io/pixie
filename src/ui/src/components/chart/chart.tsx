import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';

export interface ChartProps {
  data: GQLQueryResult;
  height: number;
  width: number;
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
