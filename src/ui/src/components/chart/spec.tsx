import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';

import * as LineChart from './line-chart';
import * as ScatterPlot from './scatter';

type ChartType = 'line' | 'scatter';

interface BaseChartSpec {
  type: ChartType;

  // The name of the table to use for the chart.
  table: string | number;
}

export interface ChartSpec extends BaseChartSpec {
  title?: string;
  overlays: BaseChartSpec[];
}

const CHART_SPEC_REGEX = /^#pl.chart:\s?(.*)$/gm;

function validateSpec(jsonSpec: string): ChartSpec | null {
  try {
    const spec = JSON.parse(jsonSpec) as ChartSpec;
    return {
      type: spec.type,
      title: spec.title,
      table: spec.table,
      overlays: spec.overlays || [],
    };
  } catch (e) {
    return null;
  }
}

function extractChartSpecFromCode(code: string): ChartSpec[] {
  const specs = [];
  code.replace(CHART_SPEC_REGEX, (orig, match) => {
    const spec = validateSpec(match);
    if (spec) {
      specs.push(spec);
    }
    return orig;
  });
  return specs;
}

interface Chart {
  chart: React.ReactNode;
  title?: string;
}

function tableFromSpec(tables: Table[], spec: BaseChartSpec): Table | null {
  switch (typeof spec.table) {
    case 'number':
      return tables[spec.table] || null;
    case 'string':
      return tables.find((table) => table.name === spec.table) || null;
    default:
      return tables[0] || null;
  }
}

function toLineChart(tables: Table[], spec: ChartSpec): React.ReactNode {
  const table = tableFromSpec(tables, spec);
  if (!table) {
    return null;
  }
  const lines = LineChart.parseData(table);
  return <LineChart.LineChart lines={lines} />;
}

function toScatterPlot(tables: Table[], spec: ChartSpec): React.ReactNode {
  const scatterTable = tableFromSpec(tables, spec);
  if (!scatterTable) {
    return null;
  }
  const overlayTables = [];
  for (const overlaySpec of (spec.overlays || [])) {
    const overlayTable = tableFromSpec(tables, overlaySpec);
    if (overlayTable) {
      overlayTables.push(overlayTable);
    }
  }
  const data = ScatterPlot.parseData([scatterTable, ...overlayTables]);
  if (!data) {
    return null;
  }
  return <ScatterPlot.ScatterPlot {...data} />;
}

function fromSpec(tables: Table[], spec: ChartSpec): Chart {
  let chart: React.ReactNode = null;
  switch (spec.type) {
    case 'line':
      chart = toLineChart(tables, spec);
      break;
    case 'scatter':
      chart = toScatterPlot(tables, spec);
      break;
  }
  return {
    chart,
    title: spec.title,
  };
}

export function chartsFromSpec(tables: Table[], code: string): Chart[] {
  const specs = extractChartSpecFromCode(code);
  return specs.map((spec) => fromSpec(tables, spec));
}
