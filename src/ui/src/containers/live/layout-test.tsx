import { buildLayout, Chart, ChartPosition, VisualizationSpecMap } from './layout';

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
    const expectedPositions: ChartPosition[] = [
      { x: 0, y: 0, w: 1, h: 1 },
      { x: 1, y: 0, w: 1, h: 1 },
      { x: 0, y: 1, w: 1, h: 1 },
    ];

    const layout = buildLayout(vegaSpecConfig);
    expect(layout.charts.length === vegaSpecConfig.size);
    const keySet = new Set();
    for (let i = 0; i < layout.charts.length; ++i) {
      expect(layout.charts[i].position === expectedPositions[i]);
      const vegaSpecKey = layout.charts[i].vegaKey;
      keySet.add(vegaSpecKey);
      // Check to make sure we're grabbing a valid spec key.
      expect(vegaSpecKey in vegaSpecConfig);
    }

    // Check to make sure we grab all of the vega spec config.
    expect(keySet.size === vegaSpecConfig.size);
  });
  it('keeps grid when specified', () => {
    // Vertically align everything.
    const chartLayout: Chart[] = [
      { position: { x: 0, y: 0, w: 1, h: 1 }, vegaKey: 'latency', description: '' },
      { position: { x: 1, y: 0, w: 1, h: 1 }, vegaKey: 'error_rate', description: '' },
      { position: { x: 0, y: 1, w: 1, h: 1 }, vegaKey: 'rps', description: '' },
    ];
    const layout = buildLayout(vegaSpecConfig, chartLayout);
    expect(layout.charts === chartLayout);
  });
});
