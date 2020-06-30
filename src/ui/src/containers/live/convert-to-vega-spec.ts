/* eslint-disable @typescript-eslint/no-use-before-define */

import { addPxTimeFormatExpression } from 'components/live-widgets/vega/timeseries-axis';
import * as _ from 'lodash';
import {
  Data, GroupMark, LineMark, Mark, Signal, Spec as VgSpec, TimeScale,
} from 'vega';
import { VisualizationSpec } from 'vega-embed';
import { compile, TopLevelSpec as VlSpec } from 'vega-lite';

import { Theme } from '@material-ui/core/styles';

import { DISPLAY_TYPE_KEY, WidgetDisplay } from './vis';

addPxTimeFormatExpression();

export const BAR_CHART_TYPE = 'pixielabs.ai/pl.vispb.BarChart';
const VEGA_CHART_TYPE = 'pixielabs.ai/pl.vispb.VegaChart';
const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
const VEGA_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega/';
const VEGA_SCHEMA = '$schema';
export const TIMESERIES_CHART_TYPE = 'pixielabs.ai/pl.vispb.TimeseriesChart';
export const COLOR_SCALE = 'color';
const HOVER_LINE_COLOR = '#4dffd4';
const HOVER_TIME_COLOR = '#121212';
const HOVER_LINE_OPACITY = 0.75;
const HOVER_LINE_DASH = [6, 6];
const HOVER_LINE_WIDTH = 2;
const LINE_WIDTH = 1.0;
const HIGHLIGHTED_LINE_WIDTH = 3.0;
const SELECTED_LINE_OPACITY = 1.0;
const UNSELECTED_LINE_OPACITY = 0.2;
const AXIS_HEIGHT = 25;

const HOVER_BULB_OFFSET = 10;
const HOVER_LINE_TEXT_OFFSET = 6;

interface XAxis {
  readonly label: string;
}

interface YAxis {
  readonly label: string;
}

interface Timeseries {
  readonly value: string;
  readonly mode?: string;
  readonly series?: string;
  readonly stackBySeries?: boolean;
}

interface TimeseriesDisplay extends WidgetDisplay {
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
  readonly timeseries: Timeseries[];
}

interface Bar {
  readonly value: string;
  readonly label: string;
  readonly stackBy?: string;
  readonly groupBy?: string;
}

interface BarDisplay extends WidgetDisplay {
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
  readonly bar: Bar;
}

interface VegaDisplay extends WidgetDisplay {
  readonly spec: string;
}

export interface VegaSpecWithProps {
  spec: VisualizationSpec;
  hasLegend: boolean;
  legendColumnName: string;
  isStacked: boolean;
  error?: Error;
}

export type ChartDisplay = TimeseriesDisplay | BarDisplay | VegaDisplay;

export function convertWidgetDisplayToVegaLiteSpec(display: ChartDisplay, source: string): VisualizationSpec {
  switch (display[DISPLAY_TYPE_KEY]) {
    case BAR_CHART_TYPE:
      return convertToBarChart(display as BarDisplay, source);
    case TIMESERIES_CHART_TYPE:
      return convertToTimeseriesChart(display as TimeseriesDisplay, source);
    case VEGA_CHART_TYPE:
      return convertToVegaChart(display as VegaDisplay, source);
    default:
      throw new Error(`Unsupported display type: ${display[DISPLAY_TYPE_KEY]}`);
  }
}

export function convertWidgetDisplayToVegaSpec(display: ChartDisplay, source: string, theme: Theme): VegaSpecWithProps {
  try {
    const vegaLiteSpec = convertWidgetDisplayToVegaLiteSpec(display, source);
    const hydratedVegaLite = hydrateSpec(vegaLiteSpec, theme) as VlSpec;
    const vegaSpec = compile(hydratedVegaLite).spec;
    return addExtrasToVegaSpec(vegaSpec, display, source);
  } catch (error) {
    return {
      spec: {},
      legendColumnName: null,
      hasLegend: false,
      isStacked: false,
      error,
    };
  }
}

// Currently only supports a single input dataframe.
// TODO(nserrino): Add support for the multi-dataframe case.
function addSources(spec: VisualizationSpec, source: string): VisualizationSpec {
  // Vega takes the data field as an array, whereas Vega-Lite takes it as a single object.
  if (spec[VEGA_SCHEMA].includes(VEGA_SCHEMA_SUBSTRING)) {
    const vgspec = spec as VgSpec;
    return { ...vgspec, data: [...(vgspec.data || []), { name: source }] };
  }
  const vlspec = spec as VlSpec;
  return { ...vlspec, data: { name: source } };
}

const TIMESERIES_TIME_COLUMN = 'time_';

const BASE_TIMESERIES_SPEC: VisualizationSpec = {
  [VEGA_SCHEMA]: VEGA_LITE_V4,
  encoding: {
    x: {
      field: TIMESERIES_TIME_COLUMN,
      type: 'temporal',
      axis: {
        grid: false,
      },
      title: null,
    },
  },
  layer: [],
};

function timeseriesDataLayer(yField: string, mark: string) {
  return {
    encoding: {
      y: { field: yField, type: 'quantitative' },
    },
    layer: [
      { mark },
    ],
  };
}

function extendEncoding(spec, field, params) {
  return {
    ...spec,
    encoding: {
      ...spec.encoding,
      [field]: {
        ...(spec.encoding ? spec.encoding[field] : {}),
        ...params,
      },
    },
  };
}

function extendXEncoding(spec, xEncoding) {
  return extendEncoding(spec, 'x', xEncoding);
}

function extendYEncoding(spec, yEncoding) {
  return extendEncoding(spec, 'y', yEncoding);
}

function extendColorEncoding(spec, colorEncoding) {
  return extendEncoding(spec, 'color', colorEncoding);
}

function extendColumnEncoding(spec, columnEncoding) {
  return extendEncoding(spec, 'column', columnEncoding);
}

function extendLayer(spec, layers) {
  return {
    ...spec,
    layer: [...(spec.layer || []), ...layers],
  };
}

function extendTransforms(spec, transforms) {
  return {
    ...spec,
    transform: [...(spec.transform || []), ...transforms],
  };
}

function randStr(length: number): string {
  // Radix is 36 since there are 26 alphabetic chars, and 10 numbers.
  const radix = 36;
  return _.range(length).map(() => _.random(radix).toString(radix)).join('');
}

// Creates the time axis configuration. The label expression calls pxTimeFormat.
function setupTimeXAxis(spec, numTicksExpr: string, separation: number, fontName: string,
  fontSize: number) {
  return extendXEncoding(spec, {
    axis: {
      grid: false,
      tickCount: {
        signal: `${numTicksExpr}`,
      },
      labelExpr: `pxTimeFormat(datum, ceil(width), ${numTicksExpr}, ${separation}, '${
        fontName}', ${fontSize})`,
      labelFlush: true,
    },
  });
}

function trimFirstAndLastTimestep(spec) {
  // NOTE(philkuz): These transforms are a hack to remove sampling artifacts created by our
  // range-agg. This should be fixed with the implementation of the window aggregate. A side-effect
  // of this hack is that any time-series created w/o range-agg will also have time-boundaries
  // removed. I'd argue that doesn't hurt the experience because those points would be missing if
  // they executed the live-view 1 sampling window earlier or later, where sampling windows
  // typically are 1-10s long.
  return extendTransforms(spec, [
    {
      joinaggregate: [
        {
          field: 'time_',
          op: 'max',
          as: 'max_time',
        },
        {
          field: 'time_',
          op: 'min',
          as: 'min_time',
        },
      ],
    },
    {
      filter: 'datum.time_ > datum.min_time && datum.time_ < datum.max_time',
    },
  ]);
}

function convertToTimeseriesChart(display: TimeseriesDisplay, source: string): VisualizationSpec {
  let spec = BASE_TIMESERIES_SPEC;
  // TODO(philkuz/reviewer) should this come from somewhere else?
  const axisLabelSeparationPx = 100;
  const axisLabelFontName = 'Roboto';
  const axisLabelFontSize = 10;

  if (display.title) {
    spec = { ...spec, title: display.title };
  }
  if (display.xAxis && display.xAxis.label) {
    spec = extendXEncoding(spec, { title: display.xAxis.label });
  }

  if (!display.timeseries) {
    throw new Error('TimeseriesChart must have one timeseries entry');
  }
  let valueField = display.timeseries[0].value;
  let colorField = '';
  if (display.timeseries.length > 1) {
    const { mode } = display.timeseries[0];
    const valueFields: string[] = [];
    for (const ts of display.timeseries) {
      if (ts.mode !== mode) {
        throw new Error('More than one timeseries in TimeseriesChart not supported if there are different mark types.');
      }
      if (ts.series) {
        throw new Error('Subseries are not supported for multiple timeseries within a TimeseriesChart');
      }
      if (!ts.value) {
        throw new Error('Each timeseries in a TimeseriesChart must have a value.');
      }
      valueFields.push(ts.value);
    }
    colorField = randStr(10);
    valueField = randStr(10);
    spec = extendTransforms(spec, [
      { fold: valueFields, as: [colorField, valueField] },
    ]);
  }

  const timeseries = display.timeseries[0];
  let mark = '';
  switch (timeseries.mode) {
    case 'MODE_POINT':
      mark = 'point';
      break;
    case 'MODE_BAR':
      mark = 'bar';
      break;
    case 'MODE_UNKNOWN':
    case 'MODE_LINE':
    default:
      mark = 'line';
  }

  if (!valueField) {
    throw new Error('No value provided for TimeseriesChart timeseries');
  }

  const layers = [];
  layers.push(timeseriesDataLayer(valueField, mark));

  if (display.yAxis && display.yAxis.label) {
    layers[0] = extendYEncoding(layers[0], { title: display.yAxis.label });
  }
  layers[0] = extendYEncoding(layers[0], { scale: { zero: false } });

  if (colorField === '' && timeseries.series) {
    colorField = timeseries.series;
  } else if (colorField === '') {
    // If there is no series provided, then we generate a series column,
    // by using the fold transform.
    // To avoid collisions, we generate a random name for the fields
    // that the fold transform creates.

    // create random alphanumeric strings of length 10.
    colorField = randStr(10);
    const newValueField = randStr(10);
    spec = extendTransforms(spec, [
      { fold: [valueField], as: [colorField, newValueField] },
    ]);
  }
  layers[0] = extendColorEncoding(layers[0], { field: colorField, type: 'nominal', legend: null });

  if (timeseries.stackBySeries) {
    if (!timeseries.series) {
      throw new Error('stackBySeries is invalid for TimeseriesChart when series is not specified');
    }
    layers[0] = extendYEncoding(layers[0], { aggregate: 'sum', stack: 'zero' });
  }

  spec = extendLayer(spec, layers);

  spec = setupTimeXAxis(spec, 'ceil(width/20)', axisLabelSeparationPx, axisLabelFontName,
    axisLabelFontSize);

  // NOTE(philkuz): Hack to remove the sampling artifacts created by our range-agg.
  spec = trimFirstAndLastTimestep(spec);

  return addSources(spec, source);
}

const BASE_BAR_SPEC: VisualizationSpec = {
  [VEGA_SCHEMA]: 'https://vega.github.io/schema/vega-lite/v4.json',
  mark: 'bar',
  encoding: {
    y: {
      type: 'quantitative',
    },
    x: {
      field: 'service',
      type: 'ordinal',
    },
  },
};

function convertToBarChart(display: BarDisplay, source: string): VisualizationSpec {
  if (!display.bar) {
    throw new Error('BarChart must have an entry for property bar');
  }
  if (!display.bar.value) {
    throw new Error('BarChart property bar must have an entry for property value');
  }
  if (!display.bar.label) {
    throw new Error('BarChart property bar must have an entry for property label');
  }

  let spec = addSources(BASE_BAR_SPEC, source);
  spec = extendXEncoding(spec, { field: display.bar.label });
  spec = extendYEncoding(spec, { field: display.bar.value });

  if (display.bar.stackBy) {
    spec = extendColorEncoding(spec, { field: display.bar.stackBy, type: 'nominal' });
    spec = extendYEncoding(spec, { aggregate: 'sum' });
  }

  if (display.yAxis && display.yAxis.label) {
    spec = extendYEncoding(spec, { title: display.yAxis.label });
  }

  // Grouped bar charts need different formatting in the x axis and title.
  if (!display.bar.groupBy) {
    if (display.xAxis && display.xAxis.label) {
      spec = extendXEncoding(spec, { title: display.xAxis.label });
    }
    if (display.title) {
      spec = { ...spec, title: display.title };
    }
    return spec;
  }

  if (display.title) {
    spec = { ...spec, title: { text: display.title, anchor: 'middle' } };
  }

  let xlabel = `${display.bar.groupBy}, ${display.bar.label}`;
  if (display.xAxis && display.xAxis.label) {
    xlabel = display.xAxis.label;
  }
  const header = { titleOrient: 'bottom', labelOrient: 'bottom', title: xlabel };

  // Use the Column encoding header instead of an x axis title to avoid per-group repetition.
  spec = extendXEncoding(spec, { title: null });
  spec = extendColumnEncoding(spec, { field: display.bar.groupBy, type: 'nominal', header });

  return spec;
}

function convertToVegaChart(display: VegaDisplay, source: string): VisualizationSpec {
  const spec: VisualizationSpec = JSON.parse(display.spec);
  if (!spec[VEGA_SCHEMA]) {
    spec[VEGA_SCHEMA] = VEGA_LITE_V4;
  }
  return addSources(spec, source);
}

function addExtrasToVegaSpec(vegaSpec, display: ChartDisplay, source: string): VegaSpecWithProps {
  switch (display[DISPLAY_TYPE_KEY]) {
    case TIMESERIES_CHART_TYPE:
      return addExtrasForTimeseries(vegaSpec, display as TimeseriesDisplay, source);
    default:
      return {
        spec: vegaSpec,
        hasLegend: false,
        legendColumnName: '',
        isStacked: false,
      };
  }
}

function extractPivotField(vegaSpec: VgSpec, display: TimeseriesDisplay): string | null {
  if (display.timeseries[0].series) {
    return display.timeseries[0].series;
  }
  if (!vegaSpec.scales) {
    return null;
  }
  for (const scale of vegaSpec.scales) {
    if (scale.name === COLOR_SCALE && scale.domain && (scale.domain as any).field) {
      return (scale.domain as any).field;
    }
  }
  return null;
}

function extractValueField(vegaSpec: VgSpec, display: TimeseriesDisplay): string | null {
  if (display.timeseries.length === 1) {
    return display.timeseries[0].value;
  }
  for (const data of vegaSpec.data) {
    if (!data.transform) {
      continue;
    }
    for (const transform of data.transform) {
      if (transform.type === 'fold') {
        return transform.as[1];
      }
    }
  }
  return '';
}

function addExtrasForTimeseries(vegaSpec, display: TimeseriesDisplay, source: string): VegaSpecWithProps {
  const isStacked: boolean = display.timeseries[0].stackBySeries;
  const pivotField: string = extractPivotField(vegaSpec, display);
  const valueField: string = extractValueField(vegaSpec, display);
  if (!pivotField || !valueField) {
    return {
      spec: vegaSpec,
      hasLegend: false,
      legendColumnName: '',
      isStacked,
    };
  }
  let newSpec = addHoverHandlersToVgSpec(vegaSpec, source, pivotField, valueField);
  newSpec = addLegendSelectHandlersToVgSpec(newSpec, pivotField, valueField);
  return {
    spec: newSpec,
    hasLegend: true,
    legendColumnName: display.timeseries[0].series || valueField,
    isStacked,
  };
}

const HOVER_VORONOI = 'hover_voronoi_layer';
const TIME_FIELD = 'time_';
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
export const HOVER_SIGNAL = 'hover_value';
export const EXTERNAL_HOVER_SIGNAL = 'external_hover_value';
export const INTERNAL_HOVER_SIGNAL = 'internal_hover_value';
export const HOVER_PIVOT_TRANSFORM = 'hover_pivot_data';
export const LEGEND_SELECT_SIGNAL = 'selected_series';
export const LEGEND_HOVER_SIGNAL = 'legend_hovered_series';
export const REVERSE_HOVER_SIGNAL = 'reverse_hovered_series';
export const REVERSE_SELECT_SIGNAL = 'reverse_selected_series';
export const REVERSE_UNSELECT_SIGNAL = 'reverse_unselect_signal';

function addLegendSelectHandlersToVgSpec(vegaSpec: VgSpec, pivotField: string, valueField: string): VgSpec {
  let spec = addLegendSignalsToVgSpec(vegaSpec, pivotField);
  spec = addOpacityTestsToLine(spec, pivotField, valueField);
  return spec;
}

function addLegendSignalsToVgSpec(vegaSpec: VgSpec, pivotField: string): VgSpec {
  const signals: Signal[] = [];
  signals.push({
    name: LEGEND_SELECT_SIGNAL,
    value: [],
  });
  signals.push({
    name: LEGEND_HOVER_SIGNAL,
    value: 'null',
  });
  signals.push({
    name: REVERSE_HOVER_SIGNAL,
    on: [
      {
        events: { source: 'view', type: 'mouseover', markname: LINE_HIT_BOX_MARK_NAME },
        update: `datum && datum["${pivotField}"]`,
      },
      {
        events: { source: 'view', type: 'mouseout', markname: LINE_HIT_BOX_MARK_NAME },
        update: 'null',
      },
    ],
  });
  signals.push({
    name: REVERSE_SELECT_SIGNAL,
    on: [
      {
        events: { source: 'view', type: 'click', markname: LINE_HIT_BOX_MARK_NAME },
        update: `datum && datum["${pivotField}"]`,
        force: true,
      },
    ],
  });
  signals.push({
    name: REVERSE_UNSELECT_SIGNAL,
    on: [
      {
        events: {
          source: 'view',
          type: 'mousedown',
          markname: LINE_HIT_BOX_MARK_NAME,
          consume: true,
          filter: `event.which === ${RIGHT_MOUSE_DOWN_CODE}`,
        },
        update: 'true',
        force: true,
      },
    ],
  });
  vegaSpec.signals.push(...signals);
  return vegaSpec;
}

function isLineMark(mark: Mark, valueField: string) {
  return mark.type === 'group'
    && mark.marks
    && mark.marks.length > 0
    && mark.marks[0]
    && mark.marks[0].encode
    && mark.marks[0].encode.update
    && mark.marks[0].encode.update.y
    && (mark.marks[0].encode.update.y as any).field
    && (mark.marks[0].encode.update.y as any).field === valueField;
}

function addOpacityTestsToLine(vegaSpec: VgSpec, pivotField: string, valueField: string): VgSpec {
  const newMarks = vegaSpec.marks.map((mark: Mark) => {
    if (!isLineMark(mark, valueField)) {
      return mark;
    }
    const groupMark = (mark as GroupMark);
    // Force the lines to be above the voronoi layer.
    groupMark.zindex = 200;
    const lineMark = (groupMark.marks[0] as LineMark);
    lineMark.encode.update.opacity = [
      {
        value: SELECTED_LINE_OPACITY,
        test: `${LEGEND_HOVER_SIGNAL} && datum["${pivotField}"] === ${LEGEND_HOVER_SIGNAL}`,
      },
      {
        value: UNSELECTED_LINE_OPACITY,
        test: `${LEGEND_SELECT_SIGNAL}.length !== 0 && indexof(${LEGEND_SELECT_SIGNAL}, datum["${pivotField}"]) === -1`,
      },
      { value: SELECTED_LINE_OPACITY },
    ];
    lineMark.encode.update.strokeWidth = [
      {
        value: HIGHLIGHTED_LINE_WIDTH,
        test: `${LEGEND_HOVER_SIGNAL} && datum["${pivotField}"] === ${LEGEND_HOVER_SIGNAL}`,
      },
      {
        value: LINE_WIDTH,
      },
    ];
    lineMark.zindex = 1;
    // Deep copy the mark because there are several nested properties that we want to inherit
    const newMark: Mark = {
      ...lineMark,
      name: LINE_HIT_BOX_MARK_NAME,
      // Deep copy encode because we need to modify them.
      encode: {
        ...lineMark.encode,
        update: {
          ...lineMark.encode.update,
          opacity: [{
            value: 0,
          }],
          strokeWidth: [{
            value: LINE_HOVER_HIT_BOX_WIDTH,
          }],
        },
      },
      zindex: lineMark.zindex + 1,
    };

    groupMark.marks.push(newMark);
    return groupMark;
  });

  vegaSpec.marks = newMarks;
  return vegaSpec;
}

export const TS_DOMAIN_SIGNAL = 'ts_domain_value';
export const EXTERNAL_TS_DOMAIN_SIGNAL = 'external_ts_domain_value';
export const INTERNAL_TS_DOMAIN_SIGNAL = 'internal_ts_domain_value';

function getLineMarkDataName(vegaSpec: VgSpec, valueField: string) {
  for (let i = 0; i < vegaSpec.marks.length; ++i) {
    const mark = vegaSpec.marks[i];
    if (isLineMark(mark, valueField)) {
      return (mark as any).from.facet.data;
    }
  }
  throw new Error('No data source found for Line Mark.');
}

function addHoverHandlersToVgSpec(vegaSpec: VgSpec, source: string, pivotField: string, valueField: string): VgSpec {
  const signalName = '_x_signal';
  vegaSpec = addTimeSeriesDomainBackupToVgSpec(vegaSpec, signalName);
  vegaSpec = addTimeseriesDomainSignalsToVgSpec(vegaSpec, signalName);
  // Get the data source used by marks.
  const markDataName = getLineMarkDataName(vegaSpec, valueField);
  vegaSpec = addHoverDataToVgSpec(vegaSpec, markDataName, pivotField, valueField);
  vegaSpec = addHoverSignalsToVgSpec(vegaSpec);
  vegaSpec = addHoverMarksToVgSpec(vegaSpec);
  return vegaSpec;
}

function addHoverDataToVgSpec(vegaSpec: VgSpec, source: string, pivotField: string, valueField: string): VgSpec {
  const data: Data[] = [];
  data.push({
    name: HOVER_PIVOT_TRANSFORM,
    source,
    transform: [
      {
        type: 'pivot',
        field: pivotField,
        value: valueField,
        groupby: [TIME_FIELD],
      },
    ],
  });
  vegaSpec.data.push(...data);
  return vegaSpec;
}

function addHoverSignalsToVgSpec(vegaSpec: VgSpec): VgSpec {
  const signals: Signal[] = [];
  // Add signal to determine hover time value for current chart.
  signals.push({
    name: INTERNAL_HOVER_SIGNAL,
    on: [
      {
        events: [
          {
            source: 'scope',
            type: 'mouseover',
            markname: HOVER_VORONOI,
          },
        ],
        // Voronoi layers store data in datum.datum instead of datum.
        update: `datum && datum.datum && {${TIME_FIELD}: datum.datum["${TIME_FIELD}"]}`,
      },
      {
        events: [
          {
            source: 'view',
            type: 'mouseout',
            // Seem to be hitting a bug in vega here where mouseout events also capture some mousemove events.
            filter: 'event.type === "mouseout"',
          },
        ],
        update: 'null',
      },
    ],
  });
  // Add signal for hover value from external chart.
  signals.push({
    name: EXTERNAL_HOVER_SIGNAL,
    value: null,
  });
  // Add signal for hover value that merges internal, and external hover values, with priority to internal.
  signals.push({
    name: HOVER_SIGNAL,
    on: [
      {
        events: [{ signal: EXTERNAL_HOVER_SIGNAL }, { signal: INTERNAL_HOVER_SIGNAL }],
        update: `${INTERNAL_HOVER_SIGNAL} || ${EXTERNAL_HOVER_SIGNAL}`,
      },
    ],
  });
  vegaSpec.signals.push(...signals);
  return vegaSpec;
}

function getXScaleFromConfig(vegaSpec: VgSpec): TimeScale {
  if (!vegaSpec.scales) {
    return null;
  }
  for (const scale of vegaSpec.scales) {
    // TODO move x to a constant.
    if (scale.name === 'x' && (scale.type === 'time' || scale.type === 'utc')) {
      return (scale as TimeScale);
    }
  }
  return null;
}

// Duplicates the Xscale so that when we update the time domain to match other charts we don't create a feedback loop.
function addTimeSeriesDomainBackupToVgSpec(vegaSpec: VgSpec, signalName: string): VgSpec {
  const xScale: TimeScale = getXScaleFromConfig(vegaSpec);

  if (!xScale) {
    return vegaSpec;
  }

  // Shallow copy so we don't interrupt the domain.
  const newXScale = { ...xScale };

  // Set the xScale domain to correspond to the TS_DOMAIN_SIGNAL.
  xScale.domainRaw = { signal: TS_DOMAIN_SIGNAL };

  newXScale.name = signalName;
  vegaSpec.scales.push(newXScale);
  return vegaSpec;
}

function addTimeseriesDomainSignalsToVgSpec(vegaSpec: VgSpec, signalName: string): VgSpec {
  const signals: Signal[] = [];
  // Add signal to determine hover time value for current chart.
  signals.push({
    name: INTERNAL_TS_DOMAIN_SIGNAL,
    on: [
      {
        events: { scale: signalName },
        update: `domain('${signalName}')`,
      },
    ],
  });
  // Add signal for hover value from external chart.
  signals.push({
    name: EXTERNAL_TS_DOMAIN_SIGNAL,
    value: null,
  });
  // Add signal for hover value that merges internal, and external hover values, with priority to internal.
  signals.push({
    name: TS_DOMAIN_SIGNAL,
    on: [
      {
        events: [{ signal: INTERNAL_TS_DOMAIN_SIGNAL }, { signal: EXTERNAL_TS_DOMAIN_SIGNAL }],
        update: `combineInternalExternal(${INTERNAL_TS_DOMAIN_SIGNAL}, ${EXTERNAL_TS_DOMAIN_SIGNAL})`,
      },
    ],
  });
  vegaSpec.signals.push(...signals);
  return vegaSpec;
}

function addHoverMarksToVgSpec(vegaSpec: VgSpec): VgSpec {
  const marks: Mark[] = [];
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
  marks.push({
    name: HOVER_RULE,
    type: 'rule',
    style: ['rule'],
    interactive: true,
    from: { data: HOVER_PIVOT_TRANSFORM },
    encode: {
      enter: {
        stroke: { value: HOVER_LINE_COLOR },
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
  marks.push({
    name: HOVER_BULB,
    type: 'symbol',
    interactive: true,
    from: { data: HOVER_PIVOT_TRANSFORM },
    encode: {
      enter: {
        fill: { value: HOVER_LINE_COLOR },
        stroke: { value: HOVER_LINE_COLOR },
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
  marks.push({
    name: HOVER_LINE_TIME,
    type: 'text',
    from: { data: HOVER_PIVOT_TRANSFORM },
    encode: {
      enter: {
        fill: { value: HOVER_TIME_COLOR },
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
    // Display text above text box.
    zindex: 1,
  });
  // Add mark for fill box around time text.
  marks.push({
    name: HOVER_LINE_TEXT_BOX,
    type: 'rect',
    from: { data: HOVER_LINE_TIME },
    encode: {
      update: {
        x: { signal: `datum.x - ((datum.bounds.x2 - datum.bounds.x1) / 2) - ${HOVER_LINE_TEXT_PADDING}` },
        y: { signal: `datum.y - ${HOVER_LINE_TEXT_PADDING}` },
        width: { signal: `datum.bounds.x2 - datum.bounds.x1 + 2 * ${HOVER_LINE_TEXT_PADDING}` },
        height: { signal: `datum.bounds.y2 - datum.bounds.y1 + 2 * ${HOVER_LINE_TEXT_PADDING}` },
        fill: { value: HOVER_LINE_COLOR },
        opacity: { signal: 'datum.opacity > 0 ? 1.0 : 0.0' },
      },
    },
  });
  // Add mark for voronoi layer.
  marks.push({
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
    // Make sure voronoi layer is on top.
    zindex: 100,
  });
  vegaSpec.marks.push(...marks);
  return vegaSpec;
}

function hydrateSpec(input, theme: Theme): VisualizationSpec {
  return {
    ...input,
    ...specsFromTheme(theme),
  };
}

function specsFromTheme(theme: Theme) {
  return {
    $schema: 'https://vega.github.io/schema/vega-lite/v4.json',
    width: 'container',
    height: 'container',
    background: theme.palette.background.default,
    padding: theme.spacing(2),
    config: {
      arc: {
        fill: '#39A8F5',
      },
      area: {
        fill: '#39A8F5',
      },
      axis: {
        labelColor: theme.palette.foreground.one,
        labelFont: 'Roboto',
        labelFontSize: 10,
        labelPadding: theme.spacing(0.5),
        tickColor: theme.palette.foreground.grey4,
        tickSize: 10,
        tickWidth: 1,
        titleColor: theme.palette.foreground.one,
        titleFont: 'Roboto',
        titleFontSize: 12,
        titleFontWeight: theme.typography.fontWeightRegular,
        titlePadding: theme.spacing(3),
      },
      axisY: {
        grid: true,
        domain: false,
        gridColor: theme.palette.foreground.grey4,
        gridWidth: 0.5,
      },
      axisX: {
        grid: false,
        domain: true,
        domainColor: theme.palette.foreground.grey4,
        tickOpacity: 0,
        tickSize: theme.spacing(0.5),
      },
      axisBand: {
        grid: false,
      },
      background: '#272822',
      group: {
        fill: '#f0f0f0',
      },
      legend: {
        fillOpacity: 1,
        labelColor: theme.palette.foreground.one,
        labelFont: 'Roboto',
        labelFontSize: 10,
        padding: theme.spacing(1),
        symbolSize: 100,
        titleColor: theme.palette.foreground.one,
        titleFontSize: 12,
      },
      view: {
        stroke: 'transparent',
      },
      line: {
        stroke: '#39A8F5',
        strokeWidth: 1,
      },
      path: {
        stroke: '#39A8F5',
        strokeWidth: 0.5,
      },
      rect: {
        fill: '#39A8F5',
      },
      range: {
        category: [
          '#21a1e7',
          '#2ca02c',
          '#98df8a',
          '#aec7e8',
          '#ff7f0e',
          '#ffbb78',
        ],
        diverging: [
          '#cc0020',
          '#e77866',
          '#f6e7e1',
          '#d6e8ed',
          '#91bfd9',
          '#1d78b5',
        ],
        heatmap: [
          '#d6e8ed',
          '#cee0e5',
          '#91bfd9',
          '#549cc6',
          '#1d78b5',
        ],
      },
      point: {
        filled: true,
        shape: 'circle',
      },
      shape: {
        stroke: '#39A8F5',
      },
      style: {
        bar: {
          binSpacing: 2,
          fill: '#39A8F5',
          stroke: null,
        },
      },
      title: {
        fontSize: 0,
      },
    },
  };
}

/* eslint-enable @typescript-eslint/no-use-before-define */
