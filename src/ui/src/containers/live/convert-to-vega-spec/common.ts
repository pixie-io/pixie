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

import {
  Axis,
  Data,
  EncodeEntryName,
  GroupMark,
  Legend,
  Mark,
  Scale,
  Signal,
  Spec as VgSpec,
  TrailEncodeEntry,
  Transforms,
} from 'vega';

import { FormatFnMetadata, getFormatFnMetadata } from 'app/containers/format-data/format-data';
import { Relation } from 'app/types/generated/vizierapi_pb';

export const VEGA_CHART_TYPE = 'types.px.dev/px.vispb.VegaChart';
export const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
export const VEGA_V5 = 'https://vega.github.io/schema/vega/v5.json';
export const VEGA_SCHEMA = '$schema';

export const TIMESERIES_CHART_TYPE = 'types.px.dev/px.vispb.TimeseriesChart';
export const BAR_CHART_TYPE = 'types.px.dev/px.vispb.BarChart';
export const PIE_CHART_TYPE = 'types.px.dev/px.vispb.PieChart';
export const HISTOGRAM_CHART_TYPE = 'types.px.dev/px.vispb.HistogramChart';
export const GAUGE_CHART_TYPE = 'types.px.dev/px.vispb.GaugeChart';
export const FLAMEGRAPH_CHART_TYPE = 'types.px.dev/px.vispb.StackTraceFlameGraph';

export const TRANSFORMED_DATA_SOURCE_NAME = 'transformed_data';

export const X_AXIS_LABEL_SEPARATION = 100; // px
export const X_AXIS_LABEL_FONT = 'Roboto';
export const X_AXIS_LABEL_FONT_SIZE = 10;
export const PX_BETWEEN_X_TICKS = 20;
export const PX_BETWEEN_Y_TICKS = 40;

export const HOVER_SIGNAL = 'hover_value';
export const EXTERNAL_HOVER_SIGNAL = 'external_hover_value';
export const INTERNAL_HOVER_SIGNAL = 'internal_hover_value';
export const HOVER_PIVOT_TRANSFORM = 'hover_pivot_data';
export const LEGEND_SELECT_SIGNAL = 'selected_series';
export const LEGEND_HOVER_SIGNAL = 'legend_hovered_series';
export const REVERSE_HOVER_SIGNAL = 'reverse_hovered_series';
export const REVERSE_SELECT_SIGNAL = 'reverse_selected_series';
export const REVERSE_UNSELECT_SIGNAL = 'reverse_unselect_signal';
export const VALUE_DOMAIN_SIGNAL = 'y_domain';
// The minimum upper bound of the Y axis domain. If the y domain upper bound is
// less than this value, we set this as the upper bound.
export const DOMAIN_MIN_UPPER_VALUE = 1;

export interface XAxis {
  readonly label: string;
}

export interface YAxis {
  readonly label: string;
}

export interface DisplayWithLabels {
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
}

export interface VegaSpecWithProps {
  spec: VgSpec;
  hasLegend: boolean;
  legendColumnName: string;
  error?: Error;
  preprocess?: (data: Array<Record<string, any>>) => Array<Record<string, any>>;
  showTooltips?: boolean;
}

export const BASE_SPEC: VgSpec = {
  [VEGA_SCHEMA]: VEGA_V5,
};

/* Vega Spec Functions */
export function addAutosize(spec: VgSpec) {
  spec.autosize = {
    type: 'fit',
    contains: 'padding',
  };
}

export function addTitle(spec: VgSpec, title: string) {
  spec.title = {
    text: title,
  };
}

export function addDataSource(spec: VgSpec, dataSpec: Data): Data {
  if (!spec.data) {
    spec.data = [];
  }
  spec.data.push(dataSpec);
  return dataSpec;
}

export function addMark(spec: VgSpec | GroupMark, markSpec: Mark): Mark {
  if (!spec.marks) {
    spec.marks = [];
  }
  spec.marks.push(markSpec);
  return markSpec;
}

export function addSignal(spec: VgSpec, sigSpec: Signal): Signal {
  if (!spec.signals) {
    spec.signals = [];
  }
  spec.signals.push(sigSpec);
  return sigSpec;
}

export function addScale(spec: VgSpec, scaleSpec: Scale): Scale {
  if (!spec.scales) {
    spec.scales = [];
  }
  spec.scales.push(scaleSpec);
  return scaleSpec;
}

export function addAxis(spec: VgSpec | GroupMark, axisSpec: Axis): Axis {
  if (!spec.axes) {
    spec.axes = [];
  }
  spec.axes.push(axisSpec);
  return axisSpec;
}

export function addLabelsToAxes(xAxis: Axis, yAxis: Axis, display: DisplayWithLabels) {
  if (display.xAxis && display.xAxis.label) {
    xAxis.title = display.xAxis.label;
  }
  if (display.yAxis && display.yAxis.label) {
    yAxis.title = display.yAxis.label;
  }
}

export function addLegend(spec: VgSpec, legendSpec: Legend): Legend {
  if (!spec.legends) {
    spec.legends = [];
  }
  spec.legends.push(legendSpec);
  return legendSpec;
}

export function addWidthHeightSignals(spec: VgSpec, widthName = 'width', heightName = 'height') {
  const widthUpdate = 'isFinite(containerSize()[0]) ? containerSize()[0] : 200';
  const heightUpdate = 'isFinite(containerSize()[1]) ? containerSize()[1] : 200';
  const widthSignal = addSignal(spec, {
    name: widthName,
    init: widthUpdate,
    on: [{
      events: 'window:resize',
      update: widthUpdate,
    }],
  });
  const heightSignal = addSignal(spec, {
    name: heightName,
    init: heightUpdate,
    on: [{
      events: 'window:resize',
      update: heightUpdate,
    }],
  });
  return { widthSignal, heightSignal };
}

export function addChildWidthHeightSignals(spec: VgSpec, widthName: string,
  heightName: string, horizontal: boolean, groupScaleName: string) {
  let widthExpr: string;
  let heightExpr: string;
  if (horizontal) {
    widthExpr = 'width';
    heightExpr = `height/domain("${groupScaleName}").length`;
  } else {
    widthExpr = `width/domain("${groupScaleName}").length`;
    heightExpr = 'height';
  }
  addSignal(spec, {
    name: widthName,
    init: widthExpr,
    on: [
      {
        events: { signal: 'width' },
        update: widthExpr,
      },
    ],
  });
  addSignal(spec, {
    name: heightName,
    init: heightExpr,
    on: [
      {
        events: { signal: 'height' },
        update: heightExpr,
      },
    ],
  });
}

export function addGridLayout(spec: VgSpec, columnDomainData: Data, horizontal: boolean) {
  spec.layout = {
    padding: 20,
    titleAnchor: {
      column: 'end',
    },
    offset: {
      columnTitle: 10,
    },
    columns: (horizontal) ? 1 : { signal: `length(data("${columnDomainData.name}"))` },
    bounds: 'full',
    align: 'all',
  };
}

export function addGridLayoutMarksForGroupedBars(
  spec: VgSpec,
  groupBy: string,
  labelField: string,
  columnDomainData: Data,
  horizontal: boolean,
  widthName: string,
  heightName: string): { groupForValueAxis: GroupMark; groupForLabelAxis: GroupMark } {
  const groupByLabelRole = (horizontal) ? 'row-title' : 'column-title';
  addMark(spec, {
    name: groupByLabelRole,
    type: 'group',
    role: groupByLabelRole,
    title: {
      text: `${groupBy}, ${labelField}`,
      orient: (horizontal) ? 'left' : 'bottom',
      offset: 10,
      style: 'grouped-bar-label-title',
    },
  });

  const valueAxisRole = (horizontal) ? 'column-footer' : 'row-header';
  const groupForValueAxis = addMark(spec, {
    name: valueAxisRole,
    type: 'group',
    role: valueAxisRole,
    encode: {
      update: (horizontal) ? { width: { signal: widthName } } : { height: { signal: heightName } },
    },
  }) as GroupMark;

  const labelAxisRole = (horizontal) ? 'row-header' : 'column-footer';
  const groupForLabelAxis = addMark(spec, {
    name: labelAxisRole,
    type: 'group',
    role: labelAxisRole,
    from: {
      data: columnDomainData.name,
    },
    sort: {
      field: `datum["${groupBy}"]`,
      order: 'ascending',
    },
    title: {
      text: {
        signal: `parent["${groupBy}"]`,
      },
      frame: 'group',
      orient: (horizontal) ? 'left' : 'bottom',
      offset: 10,
      style: 'grouped-bar-label-subtitle',
    },
    encode: {
      update: (horizontal) ? { height: { signal: heightName } } : { width: { signal: widthName } },
    },
  }) as GroupMark;
  return { groupForValueAxis, groupForLabelAxis };
}

export function extendDataTransforms(data: Data, transforms: Transforms[]) {
  if (!data.transform) {
    data.transform = [];
  }
  data.transform.push(...transforms);
}

export function extendMarkEncoding(mark: Mark, encodeEntryName: EncodeEntryName, entry: Partial<TrailEncodeEntry>) {
  if (!mark.encode) {
    mark.encode = {};
  }
  if (!mark.encode[encodeEntryName]) {
    mark.encode[encodeEntryName] = {};
  }
  mark.encode[encodeEntryName] = { ...mark.encode[encodeEntryName], ...entry };
}

export const getVegaFormatFunc = (relation: Relation, column: string): FormatFnMetadata => {
  if (!relation) {
    return null;
  }
  for (const columnInfo of relation.getColumnsList()) {
    if (column !== columnInfo.getColumnName()) {
      continue;
    }
    return getFormatFnMetadata(columnInfo.getColumnSemanticType());
  }
  return null;
};
