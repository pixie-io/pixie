import { DARK_THEME } from 'common/mui-theme';
import { convertWidgetDisplayToVegaSpec } from './convert-to-vega-spec';

function addHoverTests(spec, seriesFieldName: string, valueFieldName: string) {
  it('produces expected hover_pivot_data', () => {
    expect(spec.data).toEqual(expect.arrayContaining([
      {
        name: 'hover_pivot_data',
        // TODO(james): match the name of the transformed data source instead of any string.
        source: expect.any(String),
        transform: [{
          type: 'pivot',
          field: seriesFieldName,
          value: valueFieldName,
          groupby: ['time_'],
        }],
      },
    ]));
  });
  it('produces expected hover layer marks', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([
      expect.objectContaining({
        name: 'hover_rule_layer',
        type: 'rule',
        style: ['rule'],
        interactive: true,
        from: { data: 'hover_pivot_data' },
        encode: expect.objectContaining({
          update: expect.objectContaining({
            opacity: [
              expect.objectContaining({
                test: 'hover_value && datum && (hover_value["time_"] === datum["time_"])',
              }),
              { value: 0 },
            ],
            x: { scale: 'x', field: 'time_' },
          }),
        }),
      }),
      expect.objectContaining({
        name: 'hover_bulb_layer',
        type: 'symbol',
        interactive: true,
        from: { data: 'hover_pivot_data' },
        encode: expect.objectContaining({
          update: expect.objectContaining({
            fillOpacity: { value: 0 },
            x: { scale: 'x', field: 'time_' },
          }),
        }),
      }),
      expect.objectContaining({
        name: 'hover_time_mark',
        type: 'text',
        from: { data: 'hover_pivot_data' },
        encode: expect.objectContaining({
          update: expect.objectContaining({
            opacity: [
              expect.objectContaining({
                test: 'hover_value && datum && (hover_value["time_"] === datum["time_"])',
              }),
              { value: 0 },
            ],
            text: { signal: 'datum && timeFormat(datum["time_"], "%I:%M:%S")' },
            x: { scale: 'x', field: 'time_' },
          }),
        }),
      }),
      expect.objectContaining({
        name: 'hover_voronoi_layer',
        type: 'path',
        interactive: true,
        from: {
          data: 'hover_rule_layer',
        },
        encode: {
          update: expect.objectContaining({
            fill: { value: 'transparent' },
            isVoronoi: { value: true },
          }),
        },
        transform: [expect.objectContaining({
          type: 'voronoi',
          x: { expr: 'datum.datum.x || 0' },
          y: { expr: 'datum.datum.y || 0' },
        })],
      }),
    ]));
  });
  it('produces expected hover signals', () => {
    expect(spec.signals).toEqual(expect.arrayContaining([
      {
        name: 'internal_hover_value',
        on: expect.arrayContaining([
          {
            events: [
              {
                source: 'scope',
                type: 'mouseover',
                markname: 'hover_voronoi_layer',
              },
            ],
            update: 'datum && datum.datum && {time_: datum.datum["time_"]}',
          },
          {
            events: [
              {
                source: 'view',
                type: 'mouseout',
                filter: 'event.type === "mouseout"',
              },
            ],
            update: 'null',
          },
        ]),
      },
      {
        name: 'external_hover_value',
        value: null,
      },
      {
        name: 'hover_value',
        on: [
          {
            events: [
              { signal: 'external_hover_value' },
              { signal: 'internal_hover_value' },
            ],
            update: 'internal_hover_value || external_hover_value',
          },
        ],
      },
      {
        name: 'reverse_hovered_series',
        on: expect.arrayContaining([
          {
            events: {
              source: 'view',
              type: 'mouseover',
              markname: 'hover_line_mark_layer',
            },
            update: `datum && datum["${seriesFieldName}"]`,
          },
          {
            events: {
              source: 'view',
              type: 'mouseout',
              markname: 'hover_line_mark_layer',
            },
            update: 'null',
          },
        ]),
      },
      {
        name: 'reverse_selected_series',
        on: [
          {
            events: {
              source: 'view',
              type: 'click',
              markname: 'hover_line_mark_layer',
            },
            update: `datum && datum["${seriesFieldName}"]`,
            force: true,
          },
        ],
      },
      {
        name: 'reverse_unselect_signal',
        on: [
          {
            events: {
              source: 'view',
              type: 'mousedown',
              markname: 'hover_line_mark_layer',
              consume: true,
              filter: 'event.which === 3',
            },
            update: 'true',
            force: true,
          },
        ],
      },
    ]));
  });
}

function extractSeriesFieldName(spec): string {
  const hoverPivotDataSpec = spec.data.filter((datum) => datum.name === 'hover_pivot_data')[0];
  return hoverPivotDataSpec.transform[0].field;
}

function extractValueFieldName(spec): string {
  const hoverPivotDataSpec = spec.data.filter((datum) => datum.name === 'hover_pivot_data')[0];
  return hoverPivotDataSpec.transform[0].value;
}

describe('simple timeseries', () => {
  const valueFieldName = 'bytes_per_second';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces group mark for timeseries lines', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      marks: expect.arrayContaining([
        expect.objectContaining({
          type: 'line',
          encode: expect.objectContaining({
            update: expect.objectContaining({
              x: { scale: 'x', field: 'time_' },
              y: { scale: 'y', field: valueFieldName },
            }),
          }),
          sort: { field: 'datum["time_"]' },
          style: ['line'],
        }),
      ]),
    })]));
  });
  addHoverTests(spec, extractSeriesFieldName(spec), valueFieldName);
});

describe('timeseries with series', () => {
  const valueFieldName = 'bytes_per_second';
  const seriesFieldName = 'service';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
        series: seriesFieldName,
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces group mark for timeseries lines', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [seriesFieldName],
        }),
      },
      marks: expect.arrayContaining([
        expect.objectContaining({
          type: 'line',
          encode: expect.objectContaining({
            update: expect.objectContaining({
              x: { scale: 'x', field: 'time_' },
              y: { scale: 'y', field: valueFieldName },
            }),
          }),
          sort: { field: 'datum["time_"]' },
          style: ['line'],
        }),
      ]),
    })]));
  });
  addHoverTests(spec, seriesFieldName, valueFieldName);
});

// TODO(james/nserrino/philkuz): stack by series is currently broken, renable test once fixed.
describe.skip('timeseries with stacked series', () => {
  const valueFieldName = 'bytes_per_second';
  const seriesFieldName = 'service';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
        series: seriesFieldName,
        stackBySeries: true,
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces group mark for timeseries lines', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [seriesFieldName],
        }),
      },
      marks: expect.arrayContaining([
        expect.objectContaining({
          type: 'line',
          encode: expect.objectContaining({
            update: expect.objectContaining({
              x: { scale: 'x', field: 'time_' },
              y: { scale: 'y', field: valueFieldName },
            }),
          }),
          sort: { field: 'datum["time_"]' },
          style: ['line'],
        }),
      ]),
    })]));
  });
  addHoverTests(spec, seriesFieldName, valueFieldName);
});

describe('timeseries chart with multiple timeseries', () => {
  const firstValueField = 'bytes_per_second';
  const secondValueField = 'error_rate';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.TimeseriesChart',
    timeseries: [
      {
        value: firstValueField,
        mode: 'MODE_LINE',
      },
      {
        value: secondValueField,
        mode: 'MODE_LINE',
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  // multiple timeseries are folded under fake series and value names.
  const seriesFieldName = extractSeriesFieldName(spec);
  const valueFieldName = extractValueFieldName(spec);
  it('produces expect group mark for both timeseries', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [seriesFieldName],
        }),
      },
      marks: expect.arrayContaining([
        expect.objectContaining({
          type: 'line',
          encode: expect.objectContaining({
            update: expect.objectContaining({
              x: { scale: 'x', field: 'time_' },
              y: { scale: 'y', field: valueFieldName },
            }),
          }),
          sort: { field: 'datum["time_"]' },
          style: ['line'],
        }),
      ]),
    })]));
  });
  addHoverTests(spec, seriesFieldName, valueFieldName);
});

describe('simple bar', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces expected mark for bars', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'rect',
      encode: expect.objectContaining({
        update: expect.objectContaining({
          x: { scale: 'x', field: labelFieldName },
          y: { scale: 'y', field: valueFieldName },
          y2: { scale: 'y', value: 0 },
          width: { scale: 'x', band: 1 },
        }),
      }),
      style: ['bar'],
    })]));
  });
});

describe('bar with stackby', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const stackByFieldName = 'endpoint';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      stackBy: stackByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces expected mark for bars', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'rect',
      encode: expect.objectContaining({
        update: expect.objectContaining({
          x: { scale: 'x', field: labelFieldName },
          y: { scale: 'y', field: `sum_${valueFieldName}_end` },
          y2: { scale: 'y', field: `sum_${valueFieldName}_start` },
          fill: { scale: 'color', field: stackByFieldName },
          width: { scale: 'x', band: 1 },
        }),
      }),
      style: ['bar'],
    })]));
  });
});

describe('grouped bar', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const groupByFieldName = 'cluster';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      groupBy: groupByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  // TODO(james): add checks for column headers/footers and row headers/footers.
  it('produces group mark for bars', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [groupByFieldName],
        }),
      },
      marks: [expect.objectContaining({
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: labelFieldName },
            y: { scale: 'y', field: valueFieldName },
            y2: { scale: 'y', value: 0 },
            width: { scale: 'x', band: 1 },
          }),
        }),
        style: ['bar'],
      })],
    })]));
  });
  it('produces expected layout', () => {
    expect((spec as any).layout).toEqual(expect.objectContaining({
      bounds: 'full',
      align: 'all',
    }));
  });
});

describe('grouped bar with stackby', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const groupByFieldName = 'cluster';
  const stackByFieldName = 'endpoint';
  const input = {
    '@type': 'pixielabs.ai/pl.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      groupBy: groupByFieldName,
      stackBy: stackByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  // TODO(james): add checks for column headers/footers and row headers/footers.
  it('produces group mark for bars', () => {
    expect((spec as any).marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [groupByFieldName],
        }),
      },
      marks: [expect.objectContaining({
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: labelFieldName },
            y: { scale: 'y', field: `sum_${valueFieldName}_end` },
            y2: { scale: 'y', field: `sum_${valueFieldName}_start` },
            width: { scale: 'x', band: 1 },
          }),
        }),
        style: ['bar'],
      })],
    })]));
  });
  it('produces expected layout', () => {
    expect((spec as any).layout).toEqual(expect.objectContaining({
      bounds: 'full',
      align: 'all',
    }));
  });
});

const testInputVega = {
  $schema: 'https://vega.github.io/schema/vega/v5.json',
  width: 400,
  height: 200,
  padding: 5,
  scales: [
    {
      name: 'xscale',
      type: 'band',
      domain: { data: 'table', field: 'category' },
      range: 'width',
      padding: 0.05,
      round: true,
    },
    {
      name: 'yscale',
      domain: { data: 'table', field: 'amount' },
      nice: true,
      range: 'height',
    },
  ],
  axes: [
    { orient: 'bottom', scale: 'xscale' },
    { orient: 'left', scale: 'yscale' },
  ],
  marks: [
    {
      type: 'rect',
      from: { data: 'table' },
      encode: {
        enter: {
          x: { scale: 'xscale', field: 'category' },
          width: { scale: 'xscale', band: 1 },
          y: { scale: 'yscale', field: 'amount' },
          y2: { scale: 'yscale', value: 0 },
        },
      },
    },
  ],
};

// TODO(james): once I remove vega-lite, the VegaChart type will just pass through the inputted spec.
describe.skip('vega chart', () => {
  it('should output the inputted spec', () => {
    const input = {
      '@type': 'pixielabs.ai/pl.vispb.VegaChart',
      spec: JSON.stringify(testInputVega),
    };
    const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
    expect(spec).toStrictEqual(testInputVega);
  });
});
