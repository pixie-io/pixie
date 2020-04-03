import {VisualizationSpecMap} from 'components/vega/spec';

import {buildLayout, Chart, ChartPosition} from './layout';

const vegaSpecConfig: VisualizationSpecMap = {
  latency: {
    $schema: 'https://vega.github.io/schema/vega-lite/v2.json',
    data: {
      name: 'quantiles',
    },
    mark: 'line',
    encoding: {
      x: {
        field: 'time_',
        type: 'temporal',
        scale: {},
        title: 'Time',
      },
      y: {
        field: 'latency_p99',
        type: 'quantitative',
        scale: {},
        title: '99th Percentile Request Latency (ms)',
      },
      color: {
        field: 'pod',
        type: 'nominal',
      },
    },
  },
  error_rate: {
    $schema: 'https://vega.github.io/schema/vega-lite/v2.json',
    data: {
      name: 'quantiles',
    },
    mark: 'line',
    encoding: {
      x: {
        field: 'time_',
        type: 'temporal',
        scale: {},
        title: 'Time',
      },
      y: {
        field: 'error_rate',
        type: 'quantitative',
        scale: {},
        title: 'Error rate',
      },
      color: {
        field: 'pod',
        type: 'nominal',
      },
    },
  },
  rps: {
    $schema: 'https://vega.github.io/schema/vega-lite/v2.json',
    data: {
      name: 'quantiles',
    },
    mark: 'line',
    encoding: {
      x: {
        field: 'time_',
        type: 'temporal',
        scale: {},
        title: 'Time',
      },
      y: {
        field: 'rps',
        type: 'quantitative',
        scale: {},
        title: 'Throughput (rps)',
      },
      color: {
        field: 'pod',
        type: 'nominal',
      },
    },
  },
};

describe('BuildLayout', () => {
  it('BuildLayout tiles a grid', () => {
    const expectedPositions = {
      latency: { position: { x: 0, y: 0, w: 6, h: 3 }, description: '' },
      error_rate: { position: { x: 6, y: 0, w: 6, h: 3 }, description: '' },
      rps: { position: { x: 0, y: 3, w: 6, h: 3 }, description: '' },
    };
    const layout = buildLayout(vegaSpecConfig, {});
    expect(layout).toEqual(expectedPositions);
  });

  it('keeps grid when specified', () => {
    // Vertically align everything.
    const chartLayout = {
      latency: { position: { x: 0, y: 0, w: 1, h: 1 }, description: '' },
      error_rate: { position: { x: 1, y: 0, w: 1, h: 1 }, description: '' },
      rps: { position: { x: 0, y: 1, w: 1, h: 1 }, description: '' },
    };
    const layout = buildLayout(vegaSpecConfig, chartLayout);
    expect(layout).toEqual(chartLayout);
  });
});
