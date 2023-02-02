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
import {
  AreaMark,
  GroupMark,
  LineMark,
  Mark,
  OnEvent,
  Scale,
  Signal,
  SignalRef,
  Spec as VgSpec,
  SymbolMark,
  Transforms,
} from 'vega';

import { FormatFnMetadata } from 'app/containers/format-data/format-data';
import { WidgetDisplay } from 'app/containers/live/vis';
import { Relation } from 'app/types/generated/vizierapi_pb';

import {
  addAutosize,
  addAxis,
  addDataSource,
  addLabelsToAxes,
  addMark,
  addScale,
  addSignal,
  addTitle,
  addWidthHeightSignals,
  BASE_SPEC,
  DisplayWithLabels,
  DOMAIN_MIN_UPPER_VALUE,
  extendMarkEncoding,
  EXTERNAL_HOVER_SIGNAL,
  getVegaFormatFunc,
  HOVER_PIVOT_TRANSFORM,
  HOVER_SIGNAL,
  INTERNAL_HOVER_SIGNAL,
  LEGEND_HOVER_SIGNAL,
  LEGEND_SELECT_SIGNAL,
  PX_BETWEEN_X_TICKS,
  PX_BETWEEN_Y_TICKS,
  REVERSE_HOVER_SIGNAL,
  REVERSE_SELECT_SIGNAL,
  REVERSE_UNSELECT_SIGNAL,
  TRANSFORMED_DATA_SOURCE_NAME,
  VALUE_DOMAIN_SIGNAL,
  VegaSpecWithProps,
  X_AXIS_LABEL_FONT,
  X_AXIS_LABEL_FONT_SIZE,
  X_AXIS_LABEL_SEPARATION,
} from './common';

const AREA_MARK_OPACITY = 0.4;
const HOVER_LINE_OPACITY = 0.75;
const HOVER_LINE_DASH = [6, 6];
const HOVER_LINE_WIDTH = 2;
const LINE_WIDTH = 1.0;
const HIGHLIGHTED_LINE_WIDTH = 3.0;
const SELECTED_LINE_OPACITY = 1.0;
const UNSELECTED_LINE_OPACITY = 0.2;
const AXIS_HEIGHT = 25;

const HOVER_VORONOI = 'hover_voronoi_layer';
const HOVER_RULE = 'hover_rule_layer';
const HOVER_BULB = 'hover_bulb_layer';
const HOVER_LINE_TIME = 'hover_time_mark';
const HOVER_LINE_TEXT_BOX = 'hover_line_text_box_mark';
const HOVER_LINE_TEXT_PADDING = 3;
// Width of the clickable area of a line.
// Tweaked to a value that felt natural to click on without much effort.
const LINE_HOVER_HIT_BOX_WIDTH = 7.0;
// // Name of the mark that holds the hover interaction.
const LINE_HIT_BOX_MARK_NAME = 'hover_line_mark_layer';
const RIGHT_MOUSE_DOWN_CODE = 3;

const HOVER_BULB_OFFSET = 10;
const HOVER_LINE_TEXT_OFFSET = 6;

// Z ordering
const PLOT_GROUP_Z_LAYER = 100;
const VORONOI_Z_LAYER = 99;

export const TIME_FIELD = 'time_';
export const TS_DOMAIN_SIGNAL = 'ts_domain_value';
export const EXTERNAL_TS_DOMAIN_SIGNAL = 'external_ts_domain_value';
export const INTERNAL_TS_DOMAIN_SIGNAL = 'internal_ts_domain_value';

// Currently, only lines, points, and area are supported for timeseries.
type TimeseriesMark = LineMark | SymbolMark | AreaMark;

export interface Timeseries {
  readonly value: string;
  readonly mode?: string;
  readonly series?: string;
  readonly stackBySeries?: boolean;
}

export interface TimeseriesDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly timeseries: Timeseries[];
}

interface DomainExtentSignals {
  names: string[];
  transforms: Transforms[];
}

interface ReverseSignals {
  reverseSelectSignal: Signal;
  reverseHoverSignal: Signal;
  reverseUnselectSignal: Signal;
}

function getMarkType(mode: string): TimeseriesMark['type'] {
  switch (mode) {
    case 'MODE_POINT':
      return 'symbol';
    case 'MODE_AREA':
      return 'area';
    case 'MODE_UNKNOWN':
    case 'MODE_LINE':
    default:
      return 'line';
  }
}

function addInteractivityHitBox(spec: VgSpec | GroupMark, lineMark: TimeseriesMark, name: string): Mark {
  return addMark(spec, {
    ...lineMark,
    name,
    type: lineMark.type,
    encode: {
      ...lineMark.encode,
      update: {
        ...lineMark.encode.update,
        stroke: { value: 'transparent' },
        fill: { value: 'transparent' },
        strokeWidth: [{
          value: LINE_HOVER_HIT_BOX_WIDTH,
        }],
      },
    },
    zindex: lineMark.zindex + 1,
  });
}

function addLegendInteractivityEncodings(mark: Mark, ts: Timeseries, interactivitySelector: string) {
  extendMarkEncoding(mark, 'update', {
    opacity: [
      {
        value: UNSELECTED_LINE_OPACITY,
        test:
          `${LEGEND_SELECT_SIGNAL}.length !== 0 && indexof(${LEGEND_SELECT_SIGNAL}, ${interactivitySelector}) === -1`,
      },
      { value: SELECTED_LINE_OPACITY },
    ],
    strokeWidth: [
      {
        value: HIGHLIGHTED_LINE_WIDTH,
        test: `${LEGEND_HOVER_SIGNAL} && (${interactivitySelector} === ${LEGEND_HOVER_SIGNAL})`,
      },
      {
        value: LINE_WIDTH,
      },
    ],
  });
}

/* Data Transforms */

function createExtentTransform(field: string, signalName: string): Transforms {
  return { type: 'extent', signal: signalName, field };
}

function createExtentSignalsAndTransforms(data: string, valueFields: string[]): DomainExtentSignals {
  const names: string[] = [];
  const transforms: Transforms[] = [];
  valueFields.forEach((v) => {
    const signalName = `${data}_${v}_extent`;
    names.push(signalName);
    transforms.push(createExtentTransform(v, signalName));
  });
  return { names, transforms };
}

function timeFormatTransform(timeField: string): Transforms[] {
  return [{
    type: 'formula',
    expr: `toDate(datum["${timeField}"])`,
    as: timeField,
  }];
}

function trimFirstAndLastTimestepTransform(timeField: string): Transforms[] {
  // NOTE(philkuz): These transforms are a hack to remove sampling artifacts created by our
  // range-agg. This should be fixed with the implementation of the window aggregate. A side-effect
  // of this hack is that any time-series created w/o range-agg will also have time-boundaries
  // removed. I'd argue that doesn't hurt the experience because those points would be missing if
  // they executed the live-view 1 sampling window earlier or later, where sampling windows
  // typically are 1-10s long.
  return [
    {
      type: 'joinaggregate',
      as: [
        'min_time',
        'max_time',
      ],
      ops: [
        'min',
        'max',
      ],
      fields: [
        timeField,
        timeField,
      ],
    },
    {
      type: 'filter',
      expr: `datum.${timeField} > datum.min_time && datum.${timeField} < datum.max_time`,
    },
  ];
}

function legendDataTransform(display: TimeseriesDisplay): Transforms[] {
  // If no series in any of the timeseries, we should copy the data from the main data source,
  // and keep only the value fields + time_.
  if (display.timeseries.map((ts) => ts.series).filter((series) => series).length === 0) {
    return [{
      type: 'project',
      fields: [...display.timeseries.map((ts) => ts.value), TIME_FIELD],
    }];
  }
  if (display.timeseries.length === 1 && display.timeseries[0].series) {
    return [{
      type: 'pivot',
      field: display.timeseries[0].series,
      value: display.timeseries[0].value,
      groupby: [TIME_FIELD],
    }];
  }
  throw new Error('Multiple timeseries with subseries are not supported.');
}

function stackBySeriesTransform(
  timeField: string,
  valueField: string,
  seriesField: string,
  stackedStartField: string,
  stackedEndField: string): Transforms[] {
  const meanValueField = 'meanOfValueField';
  return [
    {
      type: 'impute',
      key: timeField,
      field: valueField,
      method: 'value',
      value: 0,
      groupby: [seriesField],
    },
    // We do a join aggregate so that we can sort the stack by the mean value per series.
    // So that the more "important" fields end up on the top of the stack.
    {
      type: 'joinaggregate',
      groupby: [seriesField],
      ops: ['mean'],
      fields: [valueField],
      as: [meanValueField],
    },
    {
      type: 'stack',
      groupby: [timeField],
      sort: { field: meanValueField, order: 'ascending' },
      field: valueField,
      as: [stackedStartField, stackedEndField],
    },
  ];
}

/* Signals */

function extendSignalHandlers(signal: Signal, on: OnEvent[]) {
  if (!signal.on) {
    signal.on = [];
  }
  signal.on.push(...on);
}

function addTimeseriesDomainSignals(spec: VgSpec): Signal {
  // Add signal for hover value from external chart.
  addSignal(spec, { name: EXTERNAL_TS_DOMAIN_SIGNAL, value: null });
  // Add signal for hover value that merges internal, and external hover values, with priority to internal.
  return addSignal(spec, {
    name: TS_DOMAIN_SIGNAL,
    on: [
      {
        events: [
          { signal: INTERNAL_TS_DOMAIN_SIGNAL },
          { signal: EXTERNAL_TS_DOMAIN_SIGNAL },
        ],
        update:
          `${EXTERNAL_TS_DOMAIN_SIGNAL} && ${EXTERNAL_TS_DOMAIN_SIGNAL}.length === 2`
          + ` ? ${EXTERNAL_TS_DOMAIN_SIGNAL} : ${INTERNAL_TS_DOMAIN_SIGNAL}`,
      },
    ],
    init: INTERNAL_TS_DOMAIN_SIGNAL,
  });
}

function createYDomainSignal(extentSignalNames: string[],
  minDomainUpperBound: number): Signal {
  const domainOpen: string[] = [];
  const domainClose: string[] = [`${minDomainUpperBound}`];
  extentSignalNames.forEach((sn) => {
    domainOpen.push(`${sn}[0]`);
    domainClose.push(`${sn}[1]`);
  });

  // Minimum of all open endpoints and maximum of all closing endpoints makes the union interval.
  const expr = `[min(${domainOpen.join(',')}), max(${domainClose.join(',')})]`;
  return {
    name: VALUE_DOMAIN_SIGNAL,
    init: expr,
    on: [
      {
        update: expr,
        events: extentSignalNames.map((sn): SignalRef => ({ signal: sn })),
      },
    ],
  };
}

function addYDomainSignal(
  spec: VgSpec, extentSignalNames: string[], minDomainUpperBound: number): Signal {
  const signal = createYDomainSignal(extentSignalNames, minDomainUpperBound);
  return addSignal(spec, signal);
}

function addHoverSelectSignals(spec: VgSpec): ReverseSignals {
  addSignal(spec, {
    name: INTERNAL_HOVER_SIGNAL,
    on: [
      {
        events: [{
          source: 'scope',
          type: 'mouseover',
          markname: HOVER_VORONOI,
        }],
        update: `datum && datum.datum && {${TIME_FIELD}: datum.datum["${TIME_FIELD}"]}`,
      },
      {
        events: [{
          source: 'view',
          type: 'mouseout',
          filter: 'event.type === "mouseout"',
        }],
        update: 'null',
      },
    ],
  });
  addSignal(spec, { name: EXTERNAL_HOVER_SIGNAL, value: null });
  addSignal(spec, {
    name: HOVER_SIGNAL,
    on: [
      {
        events: [
          { signal: INTERNAL_HOVER_SIGNAL },
          { signal: EXTERNAL_HOVER_SIGNAL },
        ],
        update: `${INTERNAL_HOVER_SIGNAL} || ${EXTERNAL_HOVER_SIGNAL}`,
      },
    ],
  });

  addSignal(spec, { name: LEGEND_SELECT_SIGNAL, value: [] });
  addSignal(spec, { name: LEGEND_HOVER_SIGNAL, value: 'null' });
  const reverseHoverSignal = addSignal(spec, { name: REVERSE_HOVER_SIGNAL });
  const reverseSelectSignal = addSignal(spec, { name: REVERSE_SELECT_SIGNAL });
  const reverseUnselectSignal = addSignal(spec, { name: REVERSE_UNSELECT_SIGNAL });
  return { reverseHoverSignal, reverseSelectSignal, reverseUnselectSignal };
}

function extendReverseSignalsWithHitBox(
  { reverseHoverSignal, reverseSelectSignal, reverseUnselectSignal }: ReverseSignals,
  hitBoxMarkName: string,
  interactivitySelector: string) {
  extendSignalHandlers(reverseHoverSignal, [
    {
      events: {
        source: 'view',
        type: 'mouseover',
        markname: hitBoxMarkName,
      },
      update: `datum && ${interactivitySelector}`,
    },
    {
      events: {
        source: 'view',
        type: 'mouseout',
        markname: hitBoxMarkName,
      },
      update: 'null',
    },
  ]);
  extendSignalHandlers(reverseSelectSignal, [
    {
      events: {
        source: 'view',
        type: 'click',
        markname: hitBoxMarkName,
      },
      update: `datum && ${interactivitySelector}`,
      force: true,
    },
  ]);
  extendSignalHandlers(reverseUnselectSignal, [
    {
      events: {
        source: 'view',
        type: 'mousedown',
        markname: hitBoxMarkName,
        consume: true,
        filter: `event.which === ${RIGHT_MOUSE_DOWN_CODE}`,
      },
      update: 'true',
      force: true,
    },
  ]);
}

/* Marks */

function addHoverMarks(spec: VgSpec, dataName: string, theme: Theme) {
  // Used by both HOVER_RULE, HOVER_LINE_TIME and HOVER_BULB.
  const hoverOpacityEncoding = [
    {
      test: `${HOVER_SIGNAL} && datum && (${HOVER_SIGNAL}["${TIME_FIELD}"] === datum["${TIME_FIELD}"])`,
      value: HOVER_LINE_OPACITY,
    },
    { value: 0 },
  ];
  // The bulb position.
  const bulbPositionSignal = { signal: `height + ${HOVER_BULB_OFFSET}` };

  // Add mark for vertical line where cursor is.
  addMark(spec, {
    name: HOVER_RULE,
    type: 'rule',
    style: ['rule'],
    interactive: true,
    from: { data: dataName },
    encode: {
      enter: {
        stroke: { value: theme.palette.success.light },
        strokeDash: { value: HOVER_LINE_DASH },
        strokeWidth: { value: HOVER_LINE_WIDTH },
      },
      update: {
        opacity: hoverOpacityEncoding,
        x: { scale: 'x', field: TIME_FIELD },
        y: { value: 0 },
        y2: { signal: `height + ${HOVER_LINE_TEXT_OFFSET}` },
      },
    },
  });
  // Bulb mark.
  addMark(spec, {
    name: HOVER_BULB,
    type: 'symbol',
    interactive: true,
    from: { data: dataName },
    encode: {
      enter: {
        fill: { value: theme.palette.success.light },
        stroke: { value: theme.palette.success.light },
        size: { value: 45 },
        shape: { value: 'circle' },
        strokeOpacity: { value: 0 },
        strokeWidth: { value: 2 },
      },
      update: {
        // fillOpacity: hoverOpacityEncoding,
        fillOpacity: { value: 0 },
        x: { scale: 'x', field: TIME_FIELD },
        y: bulbPositionSignal,
      },
    },
  });
  // Add mark for the text of the time at the bottom of the rule.
  const hoverLineTime = addMark(spec, {
    name: HOVER_LINE_TIME,
    type: 'text',
    from: { data: dataName },
    encode: {
      enter: {
        fill: { value: theme.palette.common.black },
        align: { value: 'center' },
        baseline: { value: 'top' },
        font: { value: 'Roboto' },
        fontSize: { value: 10 },
      },
      update: {
        opacity: hoverOpacityEncoding,
        text: { signal: `datum && timeFormat(datum["${TIME_FIELD}"], "%I:%M:%S")` },
        x: { scale: 'x', field: TIME_FIELD },
        y: { signal: `height + ${HOVER_LINE_TEXT_OFFSET} + ${HOVER_LINE_TEXT_PADDING}` },
      },
    },
  });
  // Add mark for fill box around time text.
  const hoverTimeBox = addMark(spec, {
    name: HOVER_LINE_TEXT_BOX,
    type: 'rect',
    from: { data: HOVER_LINE_TIME },
    encode: {
      update: {
        x: { signal: `datum.x - ((datum.bounds.x2 - datum.bounds.x1) / 2) - ${HOVER_LINE_TEXT_PADDING}` },
        y: { signal: `datum.y - ${HOVER_LINE_TEXT_PADDING}` },
        width: { signal: `datum.bounds.x2 - datum.bounds.x1 + 2 * ${HOVER_LINE_TEXT_PADDING}` },
        height: { signal: `datum.bounds.y2 - datum.bounds.y1 + 2 * ${HOVER_LINE_TEXT_PADDING}` },
        fill: { value: theme.palette.success.light },
        opacity: { signal: 'datum.opacity > 0 ? 1.0 : 0.0' },
      },
    },
    zindex: 0,
  });

  // Display text above text box.
  hoverLineTime.zindex = hoverTimeBox.zindex + 1;

  // Add mark for voronoi layer.
  addMark(spec, {
    name: HOVER_VORONOI,
    type: 'path',
    interactive: true,
    from: { data: HOVER_RULE },
    encode: {
      update: {
        fill: { value: 'transparent' },
        strokeWidth: { value: 0.35 },
        stroke: { value: 'transparent' },
        isVoronoi: { value: true },
      },
    },
    transform: [
      {
        type: 'voronoi',
        x: { expr: 'datum.datum.x || 0' },
        y: { expr: 'datum.datum.y || 0' },
        size: [{ signal: 'width' }, { signal: `height + ${AXIS_HEIGHT}` }],
      },
    ],
    zindex: VORONOI_Z_LAYER,
  });
}

/* Everything Else */

function createTSScales(
  spec: VgSpec,
  tsDomainSignal: Signal,
  yDomainSignal: Signal,
): { xScale: Scale; yScale: Scale; colorScale: Scale } {
  const xScale = addScale(spec, {
    name: 'x',
    type: 'time',
    domain: { signal: tsDomainSignal.name },
    range: [0, { signal: 'width' }],
  });
  const yScale = addScale(spec, {
    name: 'y',
    type: 'linear',
    domain: { signal: yDomainSignal.name },
    range: [{ signal: 'height' }, 0],
    zero: false,
    nice: true,
  });
  // The Color scale's domain is filled out later.
  const colorScale = addScale(spec, {
    name: 'color',
    type: 'ordinal',
    range: 'category',
  });
  return { xScale, yScale, colorScale };
}

function createTSAxes(
  spec: VgSpec,
  xScale: Scale,
  yScale: Scale,
  display: DisplayWithLabels,
  formatFuncMD: FormatFnMetadata,
) {
  const xAxis = addAxis(spec, {
    scale: xScale.name,
    orient: 'bottom',
    grid: false,
    labelFlush: true,
    tickCount: {
      signal: `ceil(width/${PX_BETWEEN_X_TICKS})`,
    },
    labelOverlap: true,
    encode: {
      labels: {
        update: {
          text: {
            signal:
              `pxTimeFormat(datum, ceil(width), ceil(width/${PX_BETWEEN_X_TICKS}),`
              + ` ${X_AXIS_LABEL_SEPARATION}, "${X_AXIS_LABEL_FONT}", ${X_AXIS_LABEL_FONT_SIZE})`,
          },
        },
      },
    },
    zindex: 0,
  });

  let formatAxis = {};
  if (formatFuncMD) {
    formatAxis = {
      encode: {
        labels: { update: { text: { signal: `${formatFuncMD.name}(datum.value)` } } },
      },
    };
  }

  const yAxis = addAxis(spec, {
    scale: yScale.name,
    orient: 'left',
    gridScale: xScale.name,
    grid: true,
    tickCount: {
      signal: `ceil(height/${PX_BETWEEN_Y_TICKS})`,
    },
    labelOverlap: true,
    zindex: 0,
    ...formatAxis,
  });
  addLabelsToAxes(xAxis, yAxis, display);
}

export function convertToTimeseriesChart(
  display: TimeseriesDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  if (!display.timeseries) {
    throw new Error('TimeseriesChart must have one timeseries entry');
  }
  const spec = { ...BASE_SPEC };
  addAutosize(spec);
  spec.style = 'cell';

  // Determine if we need to add a stack transform.
  let stackByParams: { value: string; series: string } | null = null;
  const timeseriesValues: string[] = [];
  // Do some error checks and process out values before hand.
  display.timeseries.forEach((ts) => {
    if (ts.series && display.timeseries.length > 1) {
      throw new Error('Subseries are not supported for multiple timeseries within a TimeseriesChart');
    }
    if (ts.stackBySeries && !ts.series) {
      throw new Error('Stack by series is not supported when series is not specified.');
    }

    if (ts.stackBySeries) {
      stackByParams = { value: ts.value, series: ts.series };
    }
    timeseriesValues.push(ts.value);
  });

  // Create data sources.
  const baseDataSrc = addDataSource(spec, { name: source });

  // Create the transforms that
  let domainExtents: DomainExtentSignals = createExtentSignalsAndTransforms(
    TRANSFORMED_DATA_SOURCE_NAME, timeseriesValues);
  const valueToStackBy = stackByParams ? stackByParams.value : '';
  // TODO(philkuz) need to refactor to remove these definitions.
  const stackedValueStart = `${valueToStackBy}_stacked_start`;
  const stackedValueEnd = `${valueToStackBy}_stacked_end`;
  let stackTransforms: Transforms[] = [];
  // If stackByParams are set, then we need to create a stackByTransform and update the domain extents.
  if (stackByParams) {
    stackTransforms = stackBySeriesTransform(
      TIME_FIELD, stackByParams.value, stackByParams.series, stackedValueStart, stackedValueEnd);
    domainExtents = createExtentSignalsAndTransforms(
      TRANSFORMED_DATA_SOURCE_NAME, [stackedValueStart, stackedValueEnd]);
  }

  const tsExtentTransform = createExtentTransform(TIME_FIELD, INTERNAL_TS_DOMAIN_SIGNAL);

  const { names: extentSignalNames, transforms: extentTransforms } = domainExtents;
  const transformedDataSrc = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME,
    source: baseDataSrc.name,
    transform: [
      ...timeFormatTransform(TIME_FIELD),
      ...trimFirstAndLastTimestepTransform(TIME_FIELD),
      ...stackTransforms,
      ...extentTransforms,
      tsExtentTransform,
    ],
  });

  const legendData = addDataSource(spec, {
    name: HOVER_PIVOT_TRANSFORM,
    source: transformedDataSrc.name,
    transform: [
      ...legendDataTransform(display),
    ],
  });

  // TODO(philkuz) handle case where the timeseries axes might be different sem types.
  const formatFuncMD = getVegaFormatFunc(relation, timeseriesValues[0]);

  // Create signals.
  addWidthHeightSignals(spec);
  const reverseSignals = addHoverSelectSignals(spec);
  const tsDomainSignal = addTimeseriesDomainSignals(spec);
  // Don't set a Min upper domain value for values that have format functions.
  const minUpperDomainValue = formatFuncMD ? 0 : DOMAIN_MIN_UPPER_VALUE;
  const yDomainSignal = addYDomainSignal(spec, extentSignalNames, minUpperDomainValue);

  // Create scales/axes.
  const { xScale, yScale, colorScale } = createTSScales(
    spec, tsDomainSignal, yDomainSignal);
  createTSAxes(spec, xScale, yScale, display, formatFuncMD);

  // Create marks for ts lines.
  let i = 0;
  let legendColumnName = '';
  for (const timeseries of display.timeseries) {
    let group: VgSpec | GroupMark = spec;
    let dataName = transformedDataSrc.name;
    if (timeseries.series) {
      dataName = `faceted_data_${i}`;
      group = addMark(spec, {
        name: `timeseries_group_${i}`,
        type: 'group',
        from: {
          facet: {
            name: dataName,
            data: transformedDataSrc.name,
            groupby: [timeseries.series],
          },
        },
        encode: {
          update: {
            width: {
              field: {
                group: 'width',
              },
            },
            height: {
              field: {
                group: 'height',
              },
            },
          },
        },
        zindex: PLOT_GROUP_Z_LAYER,
      });
      colorScale.domain = {
        data: transformedDataSrc.name,
        field: timeseries.series,
        sort: true,
      };
      legendColumnName = timeseries.series;
    } else {
      if (!colorScale.domain) {
        colorScale.domain = [];
      }
      (colorScale.domain as string[]).push(timeseries.value);
    }

    const markType = getMarkType(timeseries.mode);
    if (markType === 'area' && !timeseries.stackBySeries) {
      throw new Error('Area charts not supported unless stacked by series.');
    }
    const yField = (timeseries.stackBySeries) ? stackedValueEnd : timeseries.value;
    const lineMark = addMark(group, {
      name: `timeseries_line_${i}`,
      type: markType,
      style: markType,
      from: {
        data: dataName,
      },
      sort: {
        field: `datum["${TIME_FIELD}"]`,
      },
      encode: {
        update: {
          x: { scale: xScale.name, field: TIME_FIELD },
          y: { scale: yScale.name, field: yField },
          ...((markType === 'area') ? { y2: { scale: yScale.name, field: stackedValueStart } } : {}),
        },
      },
      zindex: PLOT_GROUP_Z_LAYER,
    }) as TimeseriesMark;

    if (timeseries.series) {
      extendMarkEncoding(lineMark, 'update', {
        stroke: { scale: colorScale.name, field: timeseries.series },
        ...((markType === 'area') ? {
          fill: { scale: colorScale.name, field: timeseries.series },
          fillOpacity: { value: AREA_MARK_OPACITY },
        } : {}),
      });
    } else {
      extendMarkEncoding(lineMark, 'update', {
        stroke: { scale: colorScale.name, value: timeseries.value },
      });
    }

    // NOTE(james): if there is no series given, then the selector for interactivity with the legend
    // is the name of the value field. Otherwise we use the value of the series field as the selector.
    // This will cause problems if multiple timeseries are specified with the same value field, but until we
    // support multiple tables in the same timeseries chart there isn't a problem.
    const interactivitySelector = (timeseries.series) ? `datum["${timeseries.series}"]` : `"${timeseries.value}"`;
    addLegendInteractivityEncodings(lineMark, timeseries, interactivitySelector);
    const hitBoxMark = addInteractivityHitBox(group, lineMark, `${LINE_HIT_BOX_MARK_NAME}_${i}`);
    extendReverseSignalsWithHitBox(reverseSignals, hitBoxMark.name, interactivitySelector);
    i++;
  }

  addHoverMarks(spec, legendData.name, theme);

  if (display.title) {
    addTitle(spec, display.title);
  }

  return {
    spec,
    // At the moment, timeseries always have legends.
    hasLegend: true,
    legendColumnName,
  };
}
