/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { Spec } from 'vega';

import { DARK_THEME } from 'app/components';
import { Relation, SemanticType } from 'app/types/generated/vizierapi_pb';

import {
  TRANSFORMED_DATA_SOURCE_NAME,
  VALUE_DOMAIN_SIGNAL,
} from './common';
import {
  convertWidgetDisplayToVegaSpec,
} from './convert-to-vega-spec';

function addHoverDataTest(spec: Spec, valueFieldNames: string[], seriesFieldName?: string) {
  if (seriesFieldName) {
    it('produces expected hover_data with pivot', () => {
      expect(spec.data).toEqual(expect.arrayContaining([
        {
          name: 'hover_pivot_data',
          source: expect.any(String),
          transform: [{
            type: 'pivot',
            field: seriesFieldName,
            value: valueFieldNames[0],
            groupby: ['time_'],
          }],
        },
      ]));
    });
  } else {
    it('produces expected hover_data with projection', () => {
      expect(spec.data).toEqual(expect.arrayContaining([
        {
          name: 'hover_pivot_data',
          source: expect.any(String),
          transform: [{
            type: 'project',
            fields: expect.arrayContaining([...valueFieldNames, 'time_']),
          }],
        },
      ]));
    });
  }
}

function addHoverTests(spec: Spec) {
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
            events: expect.arrayContaining([
              { signal: 'external_hover_value' },
              { signal: 'internal_hover_value' },
            ]),
            update: 'internal_hover_value || external_hover_value',
          },
        ],
      },
    ]));
  });
}

function addTimseriesAxesTest(spec: Spec, formatFuncName: string) {
  // No format func name, means we expec the y axis to not have anything
  if (formatFuncName === '') {
    it('does not have encoding on labels', () => {
      expect(spec.axes).toEqual(expect.arrayContaining([
        {
          scale: 'y',
          orient: 'left',
          gridScale: 'x',
          grid: true,
          tickCount: {
            signal: expect.stringContaining('ceil(height'),
          },
          labelOverlap: true,
          zindex: 0,
        },
      ]));
    });
  } else {
    it('has encoding on labels with formatFunc', () => {
      expect(spec.axes).toEqual(expect.arrayContaining([
        {
          scale: 'y',
          orient: 'left',
          gridScale: 'x',
          grid: true,
          tickCount: {
            signal: expect.stringContaining('ceil(height'),
          },
          labelOverlap: true,
          zindex: 0,
          encode: {
            labels: { update: { text: { signal: expect.stringContaining(formatFuncName) } } },
          },
        },
      ]));
    });
  }
}

function addTimeseriesYDomainHoverTests(spec: Spec, dsName: string, tsValues: string[]) {
  const makeSignalName = (data: string, tsValue: string) => (`${data}_${tsValue}_extent`);
  it('adds extents for timeseries values (y axis)', () => {
    expect(spec.data).toEqual(expect.arrayContaining([
      {
        name: dsName,
        source: expect.any(String),
        transform: expect.arrayContaining(tsValues.map((tsv) => ({
          type: 'extent',
          field: tsv,
          signal: makeSignalName(dsName, tsv),
        }))),
      },
    ]));
  });
  it('creates unified extent signal', () => {
    const expr = expect.any(String);
    expect(spec.signals).toEqual(expect.arrayContaining([
      {
        name: VALUE_DOMAIN_SIGNAL,
        init: expr,
        on: [
          {
            update: expr,
            events: expect.arrayContaining(tsValues.map((tsv) => ({ signal: makeSignalName(dsName, tsv) }))),
          },
        ],
      },
    ]));
  });
  it('updates y scale to use unified extent', () => {
    expect(spec.scales).toEqual(expect.arrayContaining([
      expect.objectContaining({
        name: 'y',
        domain: { signal: VALUE_DOMAIN_SIGNAL },
      }),
    ]));
  });
}

function addHoverReverseTests(spec: Spec, expectedInteractivitySelector: string) {
  it('produces expected reverse hover signals', () => {
    expect(spec.signals).toEqual(expect.arrayContaining([
      {
        name: 'reverse_hovered_series',
        on: expect.arrayContaining([
          {
            events: {
              source: 'view',
              type: 'mouseover',
              markname: 'hover_line_mark_layer_0',
            },
            update: `datum && ${expectedInteractivitySelector}`,
          },
          {
            events: {
              source: 'view',
              type: 'mouseout',
              markname: 'hover_line_mark_layer_0',
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
              markname: 'hover_line_mark_layer_0',
            },
            update: `datum && ${expectedInteractivitySelector}`,
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
              markname: 'hover_line_mark_layer_0',
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

describe('simple timeseries', () => {
  const valueFieldName = 'bytes_per_second';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces a line mark without a group', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([
      expect.objectContaining({
        type: 'line',
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: 'time_' },
            y: { scale: 'y', field: valueFieldName },
          }),
        }),
        sort: { field: 'datum["time_"]' },
        style: 'line',
      }),
    ]));
  });
  addHoverTests(spec);
  const expectedInteractivitySelector = `"${valueFieldName}"`;
  addHoverReverseTests(spec, expectedInteractivitySelector);
  addHoverDataTest(spec, [valueFieldName]);
  addTimeseriesYDomainHoverTests(spec, TRANSFORMED_DATA_SOURCE_NAME, [valueFieldName]);
  addTimseriesAxesTest(spec, /* formatFuncName */ '');
});

describe('simple timeseries with y label bytes formatting', () => {
  const valueFieldName = 'bytes_per_second';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
      },
    ],
  };

  const rel = new Relation();
  const col = new Relation.ColumnInfo();
  col.setColumnName(valueFieldName);
  col.setColumnSemanticType(SemanticType.ST_BYTES);
  rel.addColumns(col, 0);

  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME, rel);
  addTimseriesAxesTest(spec, /* formatFuncName */ 'formatBytes');
});

describe('simple timeseries with y label duration formatting', () => {
  const valueFieldName = 'latency_ms';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
    timeseries: [
      {
        value: valueFieldName,
        mode: 'MODE_LINE',
      },
    ],
  };

  const rel = new Relation();
  const col = new Relation.ColumnInfo();
  col.setColumnName(valueFieldName);
  col.setColumnSemanticType(SemanticType.ST_DURATION_NS);
  rel.addColumns(col, 0);

  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME, rel);
  addTimseriesAxesTest(spec, /* formatFuncName */ 'formatDuration');
});

describe('timeseries with series', () => {
  const valueFieldName = 'bytes_per_second';
  const seriesFieldName = 'service';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
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
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
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
          style: 'line',
        }),
      ]),
    })]));
  });
  addHoverTests(spec);
  const expectedInteractivitySelector = `datum["${seriesFieldName}"]`;
  addHoverReverseTests(spec, expectedInteractivitySelector);
  addHoverDataTest(spec, [valueFieldName], seriesFieldName);
  addTimeseriesYDomainHoverTests(spec, TRANSFORMED_DATA_SOURCE_NAME, [valueFieldName]);
});

describe('timeseries with stacked series', () => {
  const valueFieldName = 'bytes_per_second';
  const seriesFieldName = 'service';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
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
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
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
              y: { scale: 'y', field: `${valueFieldName}_stacked_end` },
            }),
          }),
          sort: { field: 'datum["time_"]' },
          style: 'line',
        }),
      ]),
    })]));
  });
  addHoverTests(spec);
  const expectedInteractivitySelector = `datum["${seriesFieldName}"]`;
  addHoverReverseTests(spec, expectedInteractivitySelector);
  addHoverDataTest(spec, [valueFieldName], seriesFieldName);
  // Instead of value fields, we use the stack values instead.
  addTimeseriesYDomainHoverTests(
    spec, TRANSFORMED_DATA_SOURCE_NAME, [`${valueFieldName}_stacked_end`, `${valueFieldName}_stacked_start`]);
});

describe('timeseries chart with multiple timeseries with different modes', () => {
  const firstValueField = 'bytes_per_second';
  const secondValueField = 'error_rate';
  const input = {
    '@type': 'types.px.dev/px.vispb.TimeseriesChart',
    timeseries: [
      {
        value: firstValueField,
        mode: 'MODE_LINE',
      },
      {
        value: secondValueField,
        mode: 'MODE_POINT',
      },
    ],
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces line mark for each timeseries', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([
      expect.objectContaining({
        type: 'line',
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: 'time_' },
            y: { scale: 'y', field: firstValueField },
          }),
        }),
        sort: { field: 'datum["time_"]' },
        style: 'line',
      }),
      expect.objectContaining({
        type: 'symbol',
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: 'time_' },
            y: { scale: 'y', field: secondValueField },
          }),
        }),
        sort: { field: 'datum["time_"]' },
        style: 'symbol',
      }),
    ]));
  });

  addHoverTests(spec);
  addHoverDataTest(spec, [firstValueField, secondValueField]);
  addTimeseriesYDomainHoverTests(spec, TRANSFORMED_DATA_SOURCE_NAME, [firstValueField, secondValueField]);

  it('produces expected reverse hover signals for each timeseries', () => {
    expect(spec.signals).toEqual(expect.arrayContaining([
      {
        name: 'reverse_hovered_series',
        on: expect.arrayContaining([
          {
            events: {
              source: 'view',
              type: 'mouseover',
              markname: 'hover_line_mark_layer_0',
            },
            update: `datum && "${firstValueField}"`,
          },
          {
            events: {
              source: 'view',
              type: 'mouseout',
              markname: 'hover_line_mark_layer_0',
            },
            update: 'null',
          },
          {
            events: {
              source: 'view',
              type: 'mouseover',
              markname: 'hover_line_mark_layer_1',
            },
            update: `datum && "${secondValueField}"`,
          },
          {
            events: {
              source: 'view',
              type: 'mouseout',
              markname: 'hover_line_mark_layer_1',
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
              markname: 'hover_line_mark_layer_0',
            },
            update: `datum && "${firstValueField}"`,
            force: true,
          },
          {
            events: {
              source: 'view',
              type: 'click',
              markname: 'hover_line_mark_layer_1',
            },
            update: `datum && "${secondValueField}"`,
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
              markname: 'hover_line_mark_layer_0',
              consume: true,
              filter: 'event.which === 3',
            },
            update: 'true',
            force: true,
          },
          {
            events: {
              source: 'view',
              type: 'mousedown',
              markname: 'hover_line_mark_layer_1',
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
});

describe('simple bar', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const input = {
    '@type': 'types.px.dev/px.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces expected mark for bars', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'rect',
      encode: expect.objectContaining({
        update: expect.objectContaining({
          y: { scale: 'y', field: labelFieldName },
          x: { scale: 'x', value: 0 },
          x2: { scale: 'x', field: valueFieldName },
          height: { scale: 'y', band: 1 },
        }),
      }),
      style: 'bar',
    })]));
  });
});

describe('simple veritcal bar', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const input = {
    '@type': 'types.px.dev/px.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      horizontal: false,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces expected mark for bars', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'rect',
      encode: expect.objectContaining({
        update: expect.objectContaining({
          x: { scale: 'x', field: labelFieldName },
          y: { scale: 'y', value: 0 },
          y2: { scale: 'y', field: valueFieldName },
          width: { scale: 'x', band: 1 },
        }),
      }),
      style: 'bar',
    })]));
  });
});

describe('bar with stackby', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const stackByFieldName = 'endpoint';
  const input = {
    '@type': 'types.px.dev/px.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      stackBy: stackByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces expected mark for bars', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'rect',
      encode: expect.objectContaining({
        update: expect.objectContaining({
          y: { scale: 'y', field: labelFieldName },
          x2: { scale: 'x', field: `sum_${valueFieldName}_end` },
          x: { scale: 'x', field: `sum_${valueFieldName}_start` },
          fill: { scale: 'color', field: stackByFieldName },
          height: { scale: 'y', band: 1 },
        }),
      }),
      style: 'bar',
    })]));
  });
});

describe('grouped bar', () => {
  const valueFieldName = 'num_errors';
  const labelFieldName = 'service';
  const groupByFieldName = 'cluster';
  const input = {
    '@type': 'types.px.dev/px.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      groupBy: groupByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces group mark for bars', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [groupByFieldName],
        }),
      },
      marks: expect.arrayContaining([
        expect.objectContaining({
          encode: expect.objectContaining({
            update: expect.objectContaining({
              y: { scale: 'y', field: labelFieldName },
              x2: { scale: 'x', field: valueFieldName },
              x: { scale: 'x', value: 0 },
              height: { scale: 'y', band: 1 },
            }),
          }),
          style: 'bar',
        }),
      ]),
    })]));
  });
  it('produces expected layout', () => {
    expect(spec.layout).toEqual(expect.objectContaining({
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
    '@type': 'types.px.dev/px.vispb.BarChart',
    bar: {
      label: labelFieldName,
      value: valueFieldName,
      groupBy: groupByFieldName,
      stackBy: stackByFieldName,
    },
  };
  const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('produces group mark for bars', () => {
    expect(spec.marks).toEqual(expect.arrayContaining([expect.objectContaining({
      type: 'group',
      from: {
        facet: expect.objectContaining({
          groupby: [groupByFieldName],
        }),
      },
      marks: expect.arrayContaining([expect.objectContaining({
        encode: expect.objectContaining({
          update: expect.objectContaining({
            x: { scale: 'x', field: `sum_${valueFieldName}_start` },
            x2: { scale: 'x', field: `sum_${valueFieldName}_end` },
            y: { scale: 'y', field: labelFieldName },
            height: { scale: 'y', band: 1 },
          }),
        }),
        style: 'bar',
      })]),
    })]));
  });
  it('produces expected layout', () => {
    expect(spec.layout).toEqual(expect.objectContaining({
      bounds: 'full',
      align: 'all',
    }));
  });
});

// TODO need support for prebinned data vs not binned.
describe('simple prebinned histogram', () => {
  const prebinFieldName = 'count';
  const binFieldName = 'latency_ms';
  const maxbins = 10;
  const minstep = 1;
  const input = {
    '@type': 'types.px.dev/px.vispb.HistogramChart',
    histogram: {
      value: binFieldName,
      prebinCount: prebinFieldName,
      maxbins,
      minstep,
    },
  };

  const binName = `bin_${binFieldName}`;
  const extentSignal = `${binName}_extent`;
  const binSignal = `${binName}_bins`;
  const binStart = binName;
  const binEnd = `${binName}_end`;
  const valueFieldCount = `${binFieldName}_count`;

  const { spec } = convertWidgetDisplayToVegaSpec(
    input,
    'mysource',
    DARK_THEME,
  );
  it('produces expected mark for bars', () => {
    expect(spec.marks).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          encode: expect.objectContaining({
            update: expect.objectContaining({
              x: { scale: 'x', field: binStart },
              x2: { scale: 'x', field: binEnd },
              y: { scale: 'y', value: 0 },
              y2: { scale: 'y', field: valueFieldCount },
            }),
          }),
          from: { data: 'transformed_data' },
          name: 'bar-mark',
          style: 'bar',
          type: 'rect',
        }),
      ]),
    );
  });

  it('produces expected scales', () => {
    expect(spec.scales).toEqual(
      expect.arrayContaining([
        {
          type: 'linear',
          bins: { signal: binSignal },
          name: 'x',
          domain: { signal: `[${binSignal}.start, ${binSignal}.stop]` },
          range: [0, { signal: 'width' }],
        },
      ]),
    );
  });

  it('has binning transforms', () => {
    expect(spec.data).toEqual(
      expect.arrayContaining([
        {
          name: TRANSFORMED_DATA_SOURCE_NAME,
          source: expect.any(String),
          transform: expect.arrayContaining([
            {
              type: 'extent',
              field: binFieldName,
              signal: extentSignal,
            },
            {
              type: 'bin',
              field: binFieldName,
              as: [binStart, binEnd],
              signal: binSignal,
              extent: {
                signal: extentSignal,
              },
              maxbins,
              minstep,
            },
            {
              type: 'aggregate',
              groupby: [binStart, binEnd],
              ops: ['sum'],
              fields: [prebinFieldName],
              as: [valueFieldCount],
            },
          ]),
        },
      ]),
    );
  });
});

const testInputVega: Spec = {
  $schema: 'https://vega.github.io/schema/vega/v5.json',
  width: 400,
  height: 200,
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

describe('vega chart', () => {
  it('should output the inputted spec hydrated with the theme', () => {
    const input = {
      '@type': 'types.px.dev/px.vispb.VegaChart',
      spec: JSON.stringify(testInputVega),
    };
    const { spec } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
    expect(spec).toEqual(
      expect.objectContaining(testInputVega),
    );
  });
});

describe('simple stacktraceFlameGraph', () => {
  const input = {
    '@type': 'types.px.dev/px.vispb.StackTraceFlameGraph',
    stacktraceColumn: 'stacktraces',
    countColumn: 'counts',
    percentageColumn: 'percent',
    pidColumn: 'pid',
    containerColumn: 'container',
    podColumn: 'pod',
    namespaceColumn: 'namespace',
    nodeColumn: 'node',
  };

  const inputData = [
    {
      stacktraces: 'st1;st2;c::trace',
      counts: 1,
      percent: 5.5,
      pid: 'pid',
      container: 'container',
      pod: 'pl/pod',
      namespace: 'namespace',
    },
    {
      stacktraces: 'st1;st4',
      counts: 2,
      percent: 2.2,
      pid: 'pid',
      container: 'container',
      pod: 'pl/pod',
      namespace: 'namespace',
    },
    {
      stacktraces: 'st2;st4',
      counts: 1,
      percent: 10,
      pid: 'pid',
      container: 'container',
      pod: 'pl/pod',
      namespace: 'namespace',
    },
    {
      stacktraces: 'st2;st4;golang.(*trace)',
      counts: 3,
      percent: 82.3,
      pid: 'pid',
      container: 'container',
      pod: 'pl/pod',
      namespace: 'namespace',
    },
    {
      stacktraces: '[k] st6',
      counts: 1,
      percent: 0,
      container: 'container',
      pod: 'pl/pod',
      namespace: 'namespace',
      node: 'node',
    },
  ];
  const { preprocess } = convertWidgetDisplayToVegaSpec(input, 'mysource', DARK_THEME);
  it('preprocesses data correctly', () => {
    const processedData = preprocess(inputData);
    expect(processedData.length).toEqual(19);
    expect(processedData).toEqual(expect.arrayContaining([
      {
        fullPath: 'all', name: 'all', count: 8, parent: null, weight: 0, color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN',
        name: 'node: UNKNOWN',
        count: 7,
        parent: 'all',
        percentage: 100,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace',
        name: 'namespace: namespace',
        count: 7,
        parent: 'all;node: UNKNOWN',
        percentage: 100,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod',
        name: 'pod: pod',
        count: 7,
        parent: 'all;node: UNKNOWN;namespace: namespace',
        percentage: 100,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container',
        name: 'container: container',
        count: 7,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod',
        percentage: 100,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid',
        name: 'pid: pid',
        count: 7,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container',
        percentage: 100,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1',
        name: 'st1',
        count: 3,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid',
        percentage: 7.7,
        weight: 0,
        color: 'other',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1;st2',
        name: 'st2',
        count: 1,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1',
        percentage: 5.5,
        weight: 0,
        color: 'other',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1;st2;c::trace',
        name: 'c::trace',
        count: 1,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1;st2',
        percentage: 5.5,
        weight: 1,
        color: 'c',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1;st4',
        name: 'st4',
        count: 2,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st1',
        percentage: 2.2,
        weight: 2,
        color: 'other',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st2',
        name: 'st2',
        count: 4,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid',
        percentage: 92.3,
        weight: 0,
        color: 'other',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st2;st4',
        name: 'st4',
        count: 4,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st2',
        percentage: 92.3,
        weight: 1,
        color: 'other',
      },
      {
        fullPath: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: '
          + 'container;pid: pid;st2;st4;golang.(*trace)',
        name: 'golang.(*trace)',
        count: 3,
        parent: 'all;node: UNKNOWN;namespace: namespace;pod: pod;container: container;pid: pid;st2;st4',
        percentage: 82.3,
        weight: 3,
        color: 'go',
      },
      {
        fullPath: 'all;node: node',
        name: 'node: node',
        count: 1,
        parent: 'all',
        percentage: 0,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: node;namespace: namespace',
        name: 'namespace: namespace',
        count: 1,
        parent: 'all;node: node',
        percentage: 0,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: node;namespace: namespace;pod: pod',
        name: 'pod: pod',
        count: 1,
        parent: 'all;node: node;namespace: namespace',
        percentage: 0,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: node;namespace: namespace;pod: pod;container: container',
        name: 'container: container',
        count: 1,
        parent: 'all;node: node;namespace: namespace;pod: pod',
        percentage: 0,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: node;namespace: namespace;pod: pod;container: container;pid: UNKNOWN',
        name: 'pid: UNKNOWN',
        count: 1,
        parent: 'all;node: node;namespace: namespace;pod: pod;container: container',
        percentage: 0,
        weight: 0,
        color: 'k8s',
      },
      {
        fullPath: 'all;node: node;namespace: namespace;pod: pod;container: container;pid: UNKNOWN;[k] st6',
        name: '[k] st6',
        count: 1,
        parent: 'all;node: node;namespace: namespace;pod: pod;container: container;pid: UNKNOWN',
        percentage: 0,
        weight: 1,
        color: 'kernel',
      },
    ]));
  });
});
