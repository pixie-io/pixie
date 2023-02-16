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
import { Spec as VgSpec } from 'vega';

import { FormatFnMetadata } from 'app/containers/format-data/format-data';
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
  extendDataTransforms,
  getVegaFormatFunc,
  HOVER_PIVOT_TRANSFORM,
  HOVER_SIGNAL,
  INTERNAL_HOVER_SIGNAL,
  LEGEND_HOVER_SIGNAL,
  LEGEND_SELECT_SIGNAL,
  REVERSE_HOVER_SIGNAL,
  REVERSE_SELECT_SIGNAL,
  REVERSE_UNSELECT_SIGNAL,
  TRANSFORMED_DATA_SOURCE_NAME,
  VegaSpecWithProps,
} from './common';
import { SHIFT_CLICK_FLAMEGRAPH_SIGNAL } from './flamegraph';
import { INTERNAL_TS_DOMAIN_SIGNAL } from './timeseries';

export interface Pie {
  value: string;
  label: string;
}

export interface PieDisplay extends WidgetDisplay {
  title: string;
  pie: Pie;
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

const ARC_MARK_NAME = 'arc_mark';

function getTooltipExpr(display: PieDisplay, formatter: FormatFnMetadata): string {
  // This is not actually JSON; Vega evaluates it and turns each key:value pair into a line in the tooltip.
  return `{
    '${display.pie.label}': datum["${display.pie.label}"],
    '${display.pie.value} (total)': ${formatter.name}(datum["sum_${display.pie.value}"]),
  }`;
}

export function convertToPieChart(
  display: PieDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  if (!display.pie || !display.pie?.value || !display.pie?.label) {
    throw new Error('Must provide pie chart definition');
  }

  const spec = {
    ...BASE_SPEC,
    config: { style: { group: { fill: 'transparent' } } }, // Remove the default gray box
  };
  addExcessSignals(spec);
  addAutosize(spec);
  addWidthHeightSignals(spec);
  const baseDataSource = addDataSource(spec, { name: source });
  const dataSource = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME,
    source: baseDataSource.name,
    transform: [],
  });

  extendDataTransforms(dataSource, [
    {
      type: 'aggregate',
      groupby: [display.pie.label],
      ops: ['sum'],
      fields: [display.pie.value],
      as: ['sum_' + display.pie.value],
    },
    {
      type: 'pie',
      field: 'sum_' + display.pie.value,
      startAngle: 0,
      endAngle: Math.PI * 2,
      sort: true,
    },
  ]);

  addScale(spec, {
    name: 'color',
    type: 'ordinal',
    range: {
      scheme: [...theme.palette.graph.category],
    },
    interpolate: 'hsl',
    domain: {
      data: dataSource.name,
      field: display.pie.label,
      sort: true,
    },
  });

  const hoverSignal = addSignal(spec, {
    name: 'arc_hover',
    on: [
      {
        events: [{ source: 'view', type: 'mouseover', markname: ARC_MARK_NAME }],
        update: `datum && { label: datum["${display.pie.label}"] }`,
      },
      {
        events: [{ source: 'view', type: 'mouseout', markname: ARC_MARK_NAME }],
        update: 'null',
      },
    ],
  });

  const formatter = getVegaFormatFunc(relation, display.pie.value);
  addMark(spec, {
    name: ARC_MARK_NAME,
    type: 'arc',
    from: { data: dataSource.name },
    clip: true,
    encode: {
      enter: {
        cursor: { value: 'help' },
        fill: { scale: 'color', field: display.pie.label },
        stroke: { value: 'white' },
        strokeWidth: { value: 1 },
        padAngle: { value: 0 },
        innerRadius: { value: 1 },
        cornerRadius: { value: 0 },
      },
      update: {
        x: { signal: 'width / 2' },
        y: { signal: 'height / 2' },
        outerRadius: { signal: '(min(width, height) / 2) - 1' },
        startAngle: { field: 'startAngle' },
        endAngle: { field: 'endAngle' },
        tooltip: { signal: getTooltipExpr(display, formatter) },
        opacity: [
          {
            test: `${hoverSignal.name} && datum && ${hoverSignal.name}.label === datum["${display.pie.label}"]`,
            value: 0.75,
          },
          { value: 1 },
        ],
      },
    },
  });

  return {
    spec,
    hasLegend: false,
    legendColumnName: '',
    showTooltips: true,
  };
}
