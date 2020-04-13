import {VisualizationSpecMap} from 'components/vega/spec';

import {addLayout, buildLayoutOld, Chart, ChartPosition} from './layout';
import {TABLE_DISPLAY_TYPE, Vis} from './vis';

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

describe('BuildLayout old', () => {
  it('BuildLayout tiles a grid', () => {
    const expectedPositions = {
      latency: { position: { x: 0, y: 0, w: 6, h: 3 }, description: '' },
      error_rate: { position: { x: 6, y: 0, w: 6, h: 3 }, description: '' },
      rps: { position: { x: 0, y: 3, w: 6, h: 3 }, description: '' },
    };
    const layout = buildLayoutOld(vegaSpecConfig, {});
    expect(layout).toEqual(expectedPositions);
  });

  it('keeps grid when specified', () => {
    // Vertically align everything.
    const chartLayout = {
      latency: { position: { x: 0, y: 0, w: 1, h: 1 }, description: '' },
      error_rate: { position: { x: 1, y: 0, w: 1, h: 1 }, description: '' },
      rps: { position: { x: 0, y: 1, w: 1, h: 1 }, description: '' },
    };
    const layout = buildLayoutOld(vegaSpecConfig, chartLayout);
    expect(layout).toEqual(chartLayout);
  });
});

const visSpec: Vis = {
  widgets: [
    {
      name: 'latency',
      func: {
        name: 'get_latency',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
    {
      name: 'error_rate',
      func: {
        name: 'get_error_rate',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
    {
      name: 'rps',
      func: {
        name: 'get_error_rate',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
  ],
};

describe('BuildLayout', () => {
  it('tiles a grid', () => {
    const expectedPositions = [
      { x: 0, y: 0, w: 6, h: 3 },
      { x: 6, y: 0, w: 6, h: 3 },
      { x: 0, y: 3, w: 6, h: 3 },
    ];

    const newVis = addLayout(visSpec);
    expect(newVis).toStrictEqual({
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => {
        return {
          ...widget,
          position: expectedPositions[i],
        };
      }),
    });
  });

  it('keeps a grid when specified', () => {
    const positions = [
      { x: 0, y: 0, w: 6, h: 3 },
      { x: 6, y: 0, w: 6, h: 3 },
      { x: 0, y: 3, w: 6, h: 3 },
    ];

    const inputVis = {
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => {
        return {
          ...widget,
          position: positions[i],
        };
      }),
    };
    const newVis = addLayout(inputVis);
    expect(newVis).toEqual(inputVis);
  });
});
