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

import { Theme } from '@mui/material/styles';
import { scheme as registerVegaScheme, Spec as VgSpec } from 'vega';

import { WidgetDisplay } from 'app/containers/live/vis';
import { Relation } from 'app/types/generated/vizierapi_pb';

import {
  addAutosize,
  addDataSource,
  addMark,
  addScale,
  addSignal,
  addWidthHeightSignals,
  BASE_SPEC,
  getVegaFormatFunc,
  HOVER_PIVOT_TRANSFORM,
  HOVER_SIGNAL,
  INTERNAL_HOVER_SIGNAL,
  LEGEND_HOVER_SIGNAL,
  LEGEND_SELECT_SIGNAL,
  REVERSE_HOVER_SIGNAL,
  REVERSE_SELECT_SIGNAL,
  REVERSE_UNSELECT_SIGNAL,
  VegaSpecWithProps,
} from './common';
import { SHIFT_CLICK_FLAMEGRAPH_SIGNAL } from './flamegraph';
import { INTERNAL_TS_DOMAIN_SIGNAL } from './timeseries';

// These could be defined (and re-defined) using the theme when rendering the chart, but these colors happen to
// work well in both themes as it is.
registerVegaScheme('gauge_mark', ['green', 'orange', 'red']);
registerVegaScheme('gauge_value', ['green', 'green', 'orange', 'red']);

export interface Gauge {
  value: string;
}

export interface GaugeDisplay extends WidgetDisplay {
  title: string;
  gauge: Gauge;
}

// The full Vega spec conversion in convert-to-vega-spec.ts adds a number of listeners without checking if they're
// relevant first. This causes Vega to log many large warnings. Adding what those listeners care about works around.
// Not doing this doesn't break anything, but doing it makes it easier to see any actually relevant warnings.
function addExcessSignals(spec: VgSpec): void {
  addSignal(spec, { name: HOVER_SIGNAL });
  addSignal(spec, { name: INTERNAL_HOVER_SIGNAL });
  addSignal(spec, { name: INTERNAL_TS_DOMAIN_SIGNAL });
  addSignal(spec, { name: LEGEND_HOVER_SIGNAL });
  addSignal(spec, { name: LEGEND_SELECT_SIGNAL });
  addSignal(spec, { name: REVERSE_HOVER_SIGNAL });
  addSignal(spec, { name: REVERSE_SELECT_SIGNAL });
  addSignal(spec, { name: REVERSE_UNSELECT_SIGNAL });
  addSignal(spec, { name: SHIFT_CLICK_FLAMEGRAPH_SIGNAL });
  addDataSource(spec, { name: HOVER_PIVOT_TRANSFORM });
}

const MAX_ANGLE = Math.PI - 1.2;
const MIN_ANGLE = -MAX_ANGLE;

export function convertToGaugeChart(
  display: GaugeDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  if (!display.gauge || !display.gauge?.value) {
    throw new Error('Must provide gauge chart definition');
  }

  const field = display.gauge.value;

  const spec = {
    ...BASE_SPEC,
    config: { style: { group: { fill: 'transparent' } } },
  };
  addExcessSignals(spec);
  addAutosize(spec);
  addWidthHeightSignals(spec);
  const baseDataSource = addDataSource(spec, { name: source });
  const dataSource = addDataSource(spec, {
    name: 'gauge',
    source: baseDataSource.name,
    transform: [
      {
        type: 'aggregate',
        fields: [field, field, field],
        ops: ['values', 'min', 'max'],
        as: ['values', 'min', 'max'],
      },
      {
        type: 'formula',
        expr: `peek(datum.values)["${field}"]`,
        as: 'latest',
      },
      {
        type: 'fold',
        fields: ['latest'],
      },
      {
        type: 'formula',
        expr: 'abs((datum.value - datum.min) / (datum.max - datum.min))',
        as: 'pct',
      },
      {
        type: 'formula',
        expr: `(${MAX_ANGLE} - ${MIN_ANGLE}) * ((datum.value - datum.min) / (datum.max - datum.min)) + ${MIN_ANGLE}`,
        as: 'shortEndAngle',
      },
      {
        type: 'pie',
        field: 'value',
        startAngle: MIN_ANGLE,
        endAngle: MAX_ANGLE,
      },
    ],
  });

  const ratingMarkSource = addDataSource(spec, {
    name: 'rating_marks',
    values: [{ value: 0.75 }, { value: 0.15 }, { value: 0.10 }],
    transform: [{
      type: 'pie',
      field: 'value',
      startAngle: MIN_ANGLE,
      endAngle: MAX_ANGLE,
    }],
  });

  addScale(spec, {
    name: 'rating_scale',
    type: 'ordinal',
    domain: { data: ratingMarkSource.name, field: 'value' },
    range: { scheme: 'gauge_mark' },
  });

  addScale(spec, {
    name: 'gauge_scale',
    type: 'threshold',
    domain: [0, 0.75, 0.9],
    range: { scheme: 'gauge_value' },
  });

  const outerDim = 'min(width, height)';
  const outerThickness = 5;
  const gap = 2;
  const innerThickness = 30;
  const radii = [ // Innermost to outermost
    `((${outerDim} / 2) - ${outerThickness} - ${gap} - ${innerThickness})`,
    `((${outerDim} / 2) - ${outerThickness} - ${gap})`,
    `((${outerDim} / 2) - ${outerThickness})`,
    `(${outerDim} / 2)`,
  ];

  addMark(spec, {
    name: 'rating_arc',
    type: 'arc',
    from: { data: ratingMarkSource.name },
    encode: {
      enter: {
        fill: { scale: 'rating_scale', field: 'value' },
      },
      update: {
        x: { signal: 'width / 2 ' },
        y: { signal: 'height / 2' },
        startAngle: { field: 'startAngle' },
        endAngle: { field: 'endAngle' },
        innerRadius: { signal: radii[2] },
        outerRadius: { signal: radii[3] },
      },
    },
  });

  addMark(spec, {
    name: 'gauge_backing_arc',
    type: 'arc',
    encode: {
      enter: {
        fill: { value: theme.palette.divider },
        startAngle: { value: MIN_ANGLE },
        endAngle: { value: MAX_ANGLE },
      },
      update: {
        x: { signal: 'width / 2' },
        y: { signal: 'height / 2' },
        innerRadius: { signal: radii[0] },
        outerRadius: { signal: radii[1] },
      },
    },
  });

  addMark(spec, {
    name: 'gauge_arc',
    type: 'arc',
    from: { data: dataSource.name },
    encode: {
      enter: {
        fill: { scale: 'gauge_scale', 'field': 'pct' },
      },
      update: {
        x: { signal: 'width / 2' },
        y: { signal: 'height / 2' },
        startAngle: { field: 'startAngle' },
        endAngle: { field: 'shortEndAngle' },
        innerRadius: { signal: radii[0] },
        outerRadius: { signal: radii[1] },
      },
    },
  });

  const formatter = getVegaFormatFunc(relation, display.gauge.value);
  addMark(spec, {
    name: 'data_label',
    from: { data: dataSource.name },
    type: 'text',
    encode: {
      enter: {
        fill: { scale: 'gauge_scale', field: 'pct' },
        text: { signal: `${formatter.name}(datum["latest"])` },
        align: { value: 'center' },
        baseline: { value: 'middle' },
        fontSize: { value: 18 }, // Note: this does not scale with the radii or the theme; nontrivial to do either.
      },
      update: {
        x: { signal: 'width / 2' },
        y: { signal: 'height / 2' },
      },
    },
  });

  addMark(spec, {
    name: 'min_label',
    from: { data: dataSource.name },
    type: 'text',
    encode: {
      enter: {
        text: { signal: `${formatter.name}(datum["min"])` },
        fill: { value: theme.palette.text.primary },
        align: { value: 'left' },
        baseline: { value: 'top' },
      },
      update: {
        x: { signal: `${outerThickness} + ${innerThickness}` },
        y: { signal: `height - ${outerThickness} - ${gap} - (${innerThickness} * 2)` },
      },
    },
  });

  addMark(spec, {
    name: 'max_label',
    from: { data: dataSource.name },
    type: 'text',
    encode: {
      enter: {
        text: { signal: `${formatter.name}(datum["max"])` },
        fill: { value: theme.palette.text.primary },
        align: { value: 'right' },
        baseline: { value: 'top' },
      },
      update: {
        x: { signal: `width - (${outerThickness} + ${innerThickness})` },
        y: { signal: `height - ${outerThickness} - ${gap} - (${innerThickness} * 2)` },
      },
    },
  });

  return {
    spec,
    hasLegend: false,
    legendColumnName: '',
  };
}
