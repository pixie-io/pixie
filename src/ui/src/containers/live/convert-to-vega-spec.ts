/* eslint-disable @typescript-eslint/no-use-before-define */
/* eslint-disable no-param-reassign */
import { Theme } from '@material-ui/core/styles';
import { formatBytes, formatDuration } from 'components/format-data/format-data';
import { addPxTimeFormatExpression } from 'components/live-widgets/vega/timeseries-axis';
import { Relation, SemanticType } from 'types/generated/vizier_pb';
import {
  Axis,
  Data,
  EncodeEntryName,
  GroupMark,
  Legend,
  LineMark,
  Mark,
  OnEvent,
  Scale,
  Signal,
  SignalRef,
  Spec as VgSpec,
  SymbolMark,
  TrailEncodeEntry,
  Transforms,
  AreaMark,
  ScaleData,
  expressionFunction,
} from 'vega';
import { vegaLite, VisualizationSpec } from 'vega-embed';
import { TopLevelSpec as VlSpec } from 'vega-lite';

import { DISPLAY_TYPE_KEY, WidgetDisplay } from './vis';

addPxTimeFormatExpression();

const VEGA_CHART_TYPE = 'pixielabs.ai/pl.vispb.VegaChart';
const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
const VEGA_V5 = 'https://vega.github.io/schema/vega/v5.json';
const VEGA_SCHEMA = '$schema';
export const TIMESERIES_CHART_TYPE = 'pixielabs.ai/pl.vispb.TimeseriesChart';
export const BAR_CHART_TYPE = 'pixielabs.ai/pl.vispb.BarChart';
export const HISTOGRAM_CHART_TYPE = 'pixielabs.ai/pl.vispb.HistogramChart';

export const COLOR_SCALE = 'color';
const HOVER_LINE_OPACITY = 0.75;
const HOVER_LINE_DASH = [6, 6];
const HOVER_LINE_WIDTH = 2;
const LINE_WIDTH = 1.0;
const HIGHLIGHTED_LINE_WIDTH = 3.0;
const SELECTED_LINE_OPACITY = 1.0;
const UNSELECTED_LINE_OPACITY = 0.2;
const AREA_MARK_OPACITY = 0.4;
const AXIS_HEIGHT = 25;
// Padding between bars, specified as fraction of step size.
const BAR_PADDING = 0.5;
const SELECTED_BAR_OPACITY = 0.9;
const UNSELECTED_BAR_OPACITY = 0.2;
const BAR_TEXT_OFFSET = 5;

const HOVER_BULB_OFFSET = 10;
const HOVER_LINE_TEXT_OFFSET = 6;
export const TRANSFORMED_DATA_SOURCE_NAME = 'transformed_data';

interface XAxis {
  readonly label: string;
}

interface YAxis {
  readonly label: string;
}

interface DisplayWithLabels {
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
}

interface Timeseries {
  readonly value: string;
  readonly mode?: string;
  readonly series?: string;
  readonly stackBySeries?: boolean;
}

interface TimeseriesDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly timeseries: Timeseries[];
}

interface Bar {
  readonly value: string;
  readonly label: string;
  readonly stackBy?: string;
  readonly groupBy?: string;
  readonly horizontal?: boolean;
}

interface BarDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly bar: Bar;
}

interface Histogram {
  readonly value: string;
  readonly maxbins?: number;
  readonly minstep?: number;
  readonly horizontal?: boolean;
  readonly prebinCount: string;
}

interface HistogramDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly histogram: Histogram;
}

interface VegaDisplay extends WidgetDisplay {
  readonly spec: string;
}

// Currently, only lines, points, and area are supported for timeseries.
type TimeseriesMark = LineMark | SymbolMark | AreaMark;

export interface VegaSpecWithProps {
  spec: VgSpec;
  hasLegend: boolean;
  legendColumnName: string;
  error?: Error;
}

interface VegaLabelFormatFunction {
  functionName: string;
  semType: SemanticType;
  formatter: (number) => string;
}

function registerFunctions(formatters: VegaLabelFormatFunction[]) {
  formatters.forEach((value: VegaLabelFormatFunction) => {
    expressionFunction(value.functionName, value.formatter);
  });
}

// const formatters: { [semType: SemanticType]: VegaLabelFormatFunction } = {
const formatters: VegaLabelFormatFunction[] = [
  {
    functionName: 'formatBytes',
    formatter: (val: number): string => {
      const data = formatBytes(val);
      return `${data.val}${data.units}`;
    },
    semType: SemanticType.ST_BYTES,
  },
  {
    functionName: 'formatDuration',
    formatter: (val: number): string => {
      const data = formatDuration(val);
      return `${data.val}${data.units}`;
    },
    semType: SemanticType.ST_DURATION_NS,
  },
];

registerFunctions(formatters);

export type ChartDisplay = TimeseriesDisplay | BarDisplay | VegaDisplay | HistogramDisplay;

function convertWidgetDisplayToSpecWithErrors(
  display: ChartDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  switch (display[DISPLAY_TYPE_KEY]) {
    case BAR_CHART_TYPE:
      return convertToBarChart(display as BarDisplay, source);
    case HISTOGRAM_CHART_TYPE:
      return convertToHistogramChart(display as HistogramDisplay, source);
    case TIMESERIES_CHART_TYPE:
      return convertToTimeseriesChart(
        display as TimeseriesDisplay,
        source,
        theme,
        relation,
      );
    case VEGA_CHART_TYPE:
      return convertToVegaChart(display as VegaDisplay);
    default:
      return {
        spec: {},
        legendColumnName: null,
        hasLegend: false,
        error: new Error(
          `Unsupported display type: ${display[DISPLAY_TYPE_KEY]}`,
        ),
      };
  }
}

export function convertWidgetDisplayToVegaSpec(
  display: ChartDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  try {
    const specWithProps = convertWidgetDisplayToSpecWithErrors(display, source, theme, relation);
    hydrateSpecWithTheme(specWithProps.spec, theme);
    return specWithProps;
  } catch (error) {
    return {
      spec: {},
      hasLegend: false,
      legendColumnName: '',
      error,
    };
  }
}

const BASE_SPEC: VgSpec = {
  [VEGA_SCHEMA]: VEGA_V5,
};

/* Vega Spec Functions */
function addAutosize(spec: VgSpec) {
  spec.autosize = {
    type: 'fit',
    contains: 'padding',
  };
}

function addTitle(spec: VgSpec, title: string) {
  spec.title = {
    text: title,
  };
}

function addDataSource(spec: VgSpec, dataSpec: Data): Data {
  if (!spec.data) {
    spec.data = [];
  }
  spec.data.push(dataSpec);
  return dataSpec;
}

function addMark(spec: VgSpec | GroupMark, markSpec: Mark): Mark {
  if (!spec.marks) {
    spec.marks = [];
  }
  spec.marks.push(markSpec);
  return markSpec;
}

function addSignal(spec: VgSpec, sigSpec: Signal): Signal {
  if (!spec.signals) {
    spec.signals = [];
  }
  spec.signals.push(sigSpec);
  return sigSpec;
}

function addScale(spec: VgSpec, scaleSpec: Scale): Scale {
  if (!spec.scales) {
    spec.scales = [];
  }
  spec.scales.push(scaleSpec);
  return scaleSpec;
}

function addAxis(spec: VgSpec | GroupMark, axisSpec: Axis): Axis {
  if (!spec.axes) {
    spec.axes = [];
  }
  spec.axes.push(axisSpec);
  return axisSpec;
}

function addLegend(spec: VgSpec, legendSpec: Legend): Legend {
  if (!spec.legends) {
    spec.legends = [];
  }
  spec.legends.push(legendSpec);
  return legendSpec;
}

/* Data Functions */
function extendDataTransforms(data: Data, transforms: Transforms[]) {
  if (!data.transform) {
    data.transform = [];
  }
  data.transform.push(...transforms);
}

/* Data Transforms */
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

/* Mark Functions */
function extendMarkEncoding(mark: Mark, encodeEntryName: EncodeEntryName, entry: Partial<TrailEncodeEntry>) {
  if (!mark.encode) {
    mark.encode = {};
  }
  if (!mark.encode[encodeEntryName]) {
    mark.encode[encodeEntryName] = {};
  }
  mark.encode[encodeEntryName] = { ...mark.encode[encodeEntryName], ...entry };
}

/* Signal Functions */
function extendSignalHandlers(signal: Signal, on: OnEvent[]) {
  if (!signal.on) {
    signal.on = [];
  }
  signal.on.push(...on);
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

function addWidthHeightSignals(spec: VgSpec, widthName = 'width', heightName = 'height') {
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

interface ReverseSignals {
  reverseSelectSignal: Signal;
  reverseHoverSignal: Signal;
  reverseUnselectSignal: Signal;
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

function addInteractivityHitBox(spec: VgSpec | GroupMark, lineMark: TimeseriesMark, name: string): Mark {
  return addMark(spec, {
    ...lineMark,
    name,
    type: lineMark.type,
    propEventsToOverlapped: true,
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

// TODO(philkuz/reviewer) should this come from somewhere else?
const X_AXIS_LABEL_SEPARATION = 100; // px
const X_AXIS_LABEL_FONT = 'Roboto';
const X_AXIS_LABEL_FONT_SIZE = 10;
const PX_BETWEEN_X_TICKS = 20;
const PX_BETWEEN_Y_TICKS = 40;

function addLabelsToAxes(xAxis: Axis, yAxis: Axis, display: DisplayWithLabels) {
  if (display.xAxis && display.xAxis.label) {
    xAxis.title = display.xAxis.label;
  }
  if (display.yAxis && display.yAxis.label) {
    yAxis.title = display.yAxis.label;
  }
}

const vegaFormatFuncForSemanticType = (semType: SemanticType): string => {
  if (semType <= SemanticType.ST_NONE) {
    return '';
  }
  for (const formatter of formatters) {
    if (formatter.semType === semType) {
      return formatter.functionName;
    }
  }
  return '';
};

const getVegaFormatFunc = (relation: Relation, column: string): string => {
  if (!relation) {
    return '';
  }
  for (const columnInfo of relation.getColumnsList()) {
    if (column !== columnInfo.getColumnName()) {
      continue;
    }
    return vegaFormatFuncForSemanticType(columnInfo.getColumnSemanticType());
  }
  return '';
};

function createTSAxes(
  spec: VgSpec,
  xScale: Scale,
  yScale: Scale,
  display: DisplayWithLabels,
  column: string,
  relation: Relation,
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

  const formatFunc = getVegaFormatFunc(relation, column);
  let formatAxis = {};
  if (formatFunc) {
    formatAxis = {
      encode: {
        labels: { update: { text: { signal: `${formatFunc}(datum.value)` } } },
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

// Z ordering
const PLOT_GROUP_Z_LAYER = 100;
const VORONOI_Z_LAYER = 99;

interface DomainExtentSignals {
  names: string[];
  transforms: Transforms[];
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

function createExtentTransform(field: string, signalName: string): Transforms {
  return { type: 'extent', signal: signalName, field };
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

function convertToTimeseriesChart(
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

  // Create signals.
  addWidthHeightSignals(spec);
  const reverseSignals = addHoverSelectSignals(spec);
  const tsDomainSignal = addTimeseriesDomainSignals(spec);
  const yDomainSignal = addYDomainSignal(spec, extentSignalNames, DOMAIN_MIN_UPPER_VALUE);

  // Create scales/axes.
  const { xScale, yScale, colorScale } = createTSScales(
    spec, tsDomainSignal, yDomainSignal);
  // TODO(philkuz) handle case where the timeseries axes might be different sem types.
  createTSAxes(spec, xScale, yScale, display, timeseriesValues[0], relation);

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
        propEventsToOverlapped: true,
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
      propEventsToOverlapped: true,
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

function addChildWidthHeightSignals(spec: VgSpec, widthName: string,
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

function addGridLayout(spec: VgSpec, columnDomainData: Data, horizontal: boolean) {
  spec.layout = {
    // TODO(james): figure out the best way to get this from the theme.
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

function addGridLayoutMarksForGroupedBars(
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

function addBarHoverSignal(
  spec: VgSpec,
  label: string,
  barMarkName: string,
  groupTest: string,
  stackTest: string,
): Signal {
  return addSignal(spec, {
    name: 'hovered_bar',
    on: [
      {
        events: [
          {
            source: 'view',
            type: 'mouseover',
            markname: barMarkName,
          },
        ],
        update: `datum && {label: datum["${label}"], group: ${groupTest}, stack: ${stackTest}}`,
      },
      {
        events: [
          {
            source: 'view',
            type: 'mouseout',
            markname: barMarkName,
          },
        ],
        update: 'null',
      },
    ],
  });
}

// Scale object used to hold a stripped down version of
// information necessary for BarChartProps.
interface InternalScale {
  domain: ScaleData | SignalRef;
  type: 'band' | 'linear';
  bins?: SignalRef;
  paddingInner?: number;
}

// Property struct for Vega BarCharts.
interface BarChartProps {
  barStart: string;
  barEnd?: string;
  groupBy?: string;
  stackBy?: string;
  horizontal: boolean;
  value: string;
  transformedDataSrc: Data;
  labelScale: InternalScale;
  display: DisplayWithLabels;
}

// The internal function to render barCharts.
function barChartInternal(chart: BarChartProps, spec: VgSpec): VegaSpecWithProps {
  if (!chart.groupBy) {
    addAutosize(spec);
  }

  let valueField = chart.value;
  let valueStartField = '';
  let valueEndField = valueField;
  if (chart.stackBy) {
    valueField = `sum_${chart.value}`;
    valueStartField = `${valueField}_start`;
    valueEndField = `${valueField}_end`;
    const extraGroupBy = [chart.barStart];
    if (chart.barEnd) {
      extraGroupBy.push(chart.barEnd);
    }
    if (chart.groupBy) {
      extraGroupBy.push(chart.groupBy);
    }
    extendDataTransforms(chart.transformedDataSrc, [
      {
        type: 'aggregate',
        groupby: [chart.stackBy, ...extraGroupBy],
        ops: ['sum'],
        fields: [chart.value],
        as: [valueField],
      },
      {
        type: 'stack',
        groupby: [...extraGroupBy],
        field: valueField,
        sort: { field: [chart.stackBy], order: ['descending'] },
        as: [valueStartField, valueEndField],
        offset: 'zero',
      },
    ]);
  }
  let columnDomainData: Data;
  if (chart.groupBy) {
    columnDomainData = addDataSource(spec, {
      name: 'column-domain',
      source: chart.transformedDataSrc.name,
      transform: [
        {
          type: 'aggregate',
          groupby: [chart.groupBy],
        },
      ],
    });
  }

  const horizontalBars = chart.horizontal;
  // Add signals.
  addWidthHeightSignals(spec);
  const widthName = (chart.groupBy) ? 'child_width' : 'width';
  const heightName = (chart.groupBy) ? 'child_height' : 'height';
  if (chart.groupBy) {
    const groupScale = addScale(spec, {
      name: 'group-scale',
      type: 'band',
      domain: {
        data: chart.transformedDataSrc.name,
        field: chart.groupBy,
        sort: true,
      },
      range: (horizontalBars) ? 'height' : 'width',
    });
    addChildWidthHeightSignals(spec, widthName, heightName, horizontalBars, groupScale.name);
  }
  const barMarkName = 'bar-mark';
  const groupTest = (datum: string) => ((chart.groupBy) ? `${datum}["${chart.groupBy}"]` : 'null');
  const stackTest = (datum: string) => ((chart.stackBy) ? `${datum}["${chart.stackBy}"]` : 'null');
  const hoverSignal = addBarHoverSignal(spec, chart.barStart, barMarkName, groupTest('datum'), stackTest('datum'));
  const isHovered = (datum: string) => `${hoverSignal.name}.label === ${datum}["${chart.barStart}"] && `
    + `${hoverSignal.name}.group === ${groupTest(datum)} && `
    + `${hoverSignal.name}.stack === ${stackTest(datum)}`;

  // Add scales.
  const labelScale = addScale(spec, {
    name: (horizontalBars) ? 'y' : 'x',
    range: (horizontalBars) ? [{ signal: heightName }, 0] : [0, { signal: widthName }],
    ...chart.labelScale,
  });

  const valueScale = addScale(spec, {
    name: (horizontalBars) ? 'x' : 'y',
    type: 'linear',
    domain: {
      data: chart.transformedDataSrc.name,
      fields: (valueStartField) ? [valueStartField, valueEndField] : [valueField],
    },
    range: (horizontalBars) ? [0, { signal: widthName }] : [{ signal: heightName }, 0],
    nice: true,
    zero: true,
  });

  const colorScale = addScale(spec, {
    name: 'color',
    type: 'ordinal',
    range: 'category',
    domain: (!chart.stackBy) ? [valueField] : {
      data: chart.transformedDataSrc.name,
      field: chart.stackBy,
      sort: true,
    },
  });

  // Add marks.
  let group: VgSpec | GroupMark = spec;
  let dataName = chart.transformedDataSrc.name;
  let groupForValueAxis: VgSpec | GroupMark = spec;
  let groupForLabelAxis: VgSpec | GroupMark = spec;
  if (chart.groupBy) {
    // We use vega's grid layout functionality to plot grouped bars.
    ({ groupForValueAxis, groupForLabelAxis } = addGridLayoutMarksForGroupedBars(
      spec, chart.groupBy, chart.barStart, columnDomainData, horizontalBars, widthName, heightName));
    addGridLayout(spec, columnDomainData, horizontalBars);
    dataName = 'facetedData';
    group = addMark(spec, {
      name: 'barGroup',
      type: 'group',
      style: 'cell',
      from: {
        facet: {
          name: dataName,
          data: chart.transformedDataSrc.name,
          groupby: [chart.groupBy],
        },
      },
      sort: {
        field: [`datum["${chart.groupBy}"]`],
        order: ['ascending'],
      },
      encode: {
        update: {
          width: {
            signal: widthName,
          },
          height: {
            signal: heightName,
          },
        },
      },
      axes: [
        {
          scale: valueScale.name,
          orient: (horizontalBars) ? 'bottom' : 'left',
          grid: true,
          gridScale: labelScale.name,
          tickCount: { signal: `ceil(${heightName}/${PX_BETWEEN_Y_TICKS})` },
          labelOverlap: true,
          labels: false,
          ticks: false,
        },
      ],
    }) as GroupMark;
  }

  const barMark = addMark(group, {
    name: barMarkName,
    type: 'rect',
    style: 'bar',
    from: {
      data: dataName,
    },
    encode: {
      update: {
        fill: {
          scale: colorScale.name,
          ...((chart.stackBy) ? { field: chart.stackBy } : { value: valueField }),
        },
        opacity: [
          { test: `!${hoverSignal.name}`, value: SELECTED_BAR_OPACITY },
          {
            test: `${hoverSignal.name} && datum && ${isHovered('datum')}`,
            value: SELECTED_BAR_OPACITY,
          },
          { value: UNSELECTED_BAR_OPACITY },
        ],
      },
    },
  });

  let barWidthEncodingKey = (horizontalBars) ? 'height' : 'width';
  let barWidthEncodingValue: any = { scale: labelScale.name, band: 1 };
  if (chart.barEnd) {
    barWidthEncodingKey = (horizontalBars) ? 'y2' : 'x2';
    barWidthEncodingValue = { scale: labelScale.name, field: chart.barEnd };
  }

  extendMarkEncoding(barMark, 'update', {
    // Bottom of the bar. If there's no "startField", then just 0.
    [(horizontalBars) ? 'x' : 'y']: {
      scale: valueScale.name,
      ...((valueStartField) ? { field: valueStartField } : { value: 0 }),
    },
    // Top of the bar.
    [(horizontalBars) ? 'x2' : 'y2']: {
      scale: valueScale.name,
      field: valueEndField,
    },
    // The label of the bar.
    [(horizontalBars) ? 'y' : 'x']: {
      scale: labelScale.name,
      field: chart.barStart,
    },
    // The width of the bar.
    [barWidthEncodingKey]: barWidthEncodingValue,
  });

  addMark(group, {
    name: 'bar-value-text',
    type: 'text',
    style: 'bar-value-text',
    from: {
      data: barMarkName,
    },
    encode: {
      enter: {
        text: { field: `datum["${valueField}"]` },
        ...((horizontalBars)
          ? {
            x: { field: 'x2', offset: BAR_TEXT_OFFSET },
            y: { field: 'y', offset: { field: 'height', mult: 0.5 } },
            baseline: { value: 'middle' },
            align: { value: 'left' },
          }
          : {
            x: { field: 'x', offset: { field: 'width', mult: 0.5 } },
            y: { field: 'y', offset: -BAR_TEXT_OFFSET },
            baseline: { value: 'bottom' },
            align: { value: 'center' },
          }),
      },
      update: {
        opacity: [
          { test: `${hoverSignal.name} && datum && datum.datum && ${isHovered('datum.datum')}`, value: 1.0 },
          { value: 0.0 },
        ],
      },
    },
  });

  const xHasGrid = !chart.groupBy && !!horizontalBars;
  const yHasGrid = !chart.groupBy && !horizontalBars;

  const xAxis = addAxis((horizontalBars) ? groupForValueAxis : groupForLabelAxis, {
    scale: (horizontalBars) ? valueScale.name : labelScale.name,
    orient: 'bottom',
    grid: xHasGrid,
    gridScale: (xHasGrid) ? labelScale.name : null,
    labelAlign: (horizontalBars) ? 'center' : 'right',
    labelAngle: (horizontalBars) ? 0 : 270,
    labelBaseline: 'middle',
    labelOverlap: true,
    labelSeparation: 3,
    // Tick count is only used if this is the valueAxis.
    tickCount: {
      signal: `ceil(${widthName}/${PX_BETWEEN_Y_TICKS})`,
    },
  });
  const yAxis = addAxis((horizontalBars) ? groupForLabelAxis : groupForValueAxis, {
    scale: (horizontalBars) ? labelScale.name : valueScale.name,
    orient: 'left',
    grid: yHasGrid,
    gridScale: yHasGrid ? labelScale.name : null,
    labelOverlap: true,
    // Tick count is only used if this is the valueAxis.
    tickCount: {
      signal: `ceil(${heightName}/${PX_BETWEEN_Y_TICKS})`,
    },
  });
  addLabelsToAxes(xAxis, yAxis, chart.display);

  if (chart.stackBy) {
    addLegend(spec, {
      fill: colorScale.name,
      symbolType: 'square',
      title: chart.stackBy,
      encode: {
        symbols: {
          update: {
            stroke: {
              value: null,
            },
          },
        },
      },
    });
  }

  if (chart.display.title) {
    addTitle(spec, chart.display.title);
  }

  return {
    spec,
    hasLegend: false,
    legendColumnName: '',
  };
}

function convertToBarChart(display: BarDisplay, source: string): VegaSpecWithProps {
  if (!display.bar) {
    throw new Error('BarChart must have an entry for property bar');
  }
  if (!display.bar.value) {
    throw new Error('BarChart property bar must have an entry for property value');
  }
  if (!display.bar.label) {
    throw new Error('BarChart property bar must have an entry for property label');
  }

  const spec = { ...BASE_SPEC, style: 'cell' };
  // Add data and transforms.
  const baseDataSrc = addDataSource(spec, { name: source });
  const transformedDataSrc = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME, source: baseDataSrc.name, transform: [],
  });
  // Horizontal should be default.
  const horizontalBars = (display.bar.horizontal === undefined) ? true : display.bar.horizontal;

  return barChartInternal({
    barStart: display.bar.label,
    horizontal: horizontalBars,
    value: display.bar.value,
    transformedDataSrc,
    groupBy: display.bar.groupBy,
    stackBy: display.bar.stackBy,
    labelScale: {
      domain: {
        data: transformedDataSrc.name,
        field: display.bar.label,
        sort: true,
      },
      paddingInner: BAR_PADDING,
      type: 'band',
    },
    display,
  }, spec);
}

function convertToHistogramChart(display: HistogramDisplay, source: string): VegaSpecWithProps {
  if (!display.histogram) {
    throw new Error('HistogramChart must have an entry for property histogram');
  }
  if (!display.histogram.value) {
    throw new Error('HistogramChart property histogram must have an entry for property value');
  }
  // TODO(philkuz) support non-prebinned histograms.
  if (!display.histogram.prebinCount) {
    throw new Error('HistogramChart property histogram must have an entry for the prebinField');
  }

  const spec = { ...BASE_SPEC, style: 'cell' };

  // Add data and transforms.
  const baseDataSrc = addDataSource(spec, { name: source });
  const transformedDataSrc = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME, source: baseDataSrc.name, transform: [],
  });

  const binName = `bin_${display.histogram.value}`;
  const extentSignal = `${binName}_extent`;
  const binSignal = `${binName}_bins`;
  const binStart = binName;
  const binEnd = `${binName}_end`;
  const countField = `${display.histogram.value}_count`;

  const DEFAULT_MAX_BINS = 10;
  const DEFAULT_MIN_STEP = 0.0;
  // Groups which to aggregate by, will include groupby and stackby if we add those to histogram.
  const groups: string[] = [];
  // Setup histograms.
  extendDataTransforms(transformedDataSrc, [
    {
      type: 'extent',
      field: display.histogram.value,
      signal: extentSignal,
    },
    {
      type: 'bin',
      field: display.histogram.value,
      as: [binStart, binEnd],
      signal: binSignal,
      extent: {
        signal: extentSignal,
      },
      maxbins: display.histogram.maxbins || DEFAULT_MAX_BINS,
      minstep: display.histogram.minstep || DEFAULT_MIN_STEP,
    },
    {
      type: 'aggregate',
      groupby: [binStart, binEnd, ...groups],
      ops: ['sum'],
      fields: [display.histogram.prebinCount],
      as: [countField],
    },
  ]);

  return barChartInternal({
    barStart: binStart,
    barEnd: binEnd,
    horizontal: display.histogram.horizontal,
    value: countField,
    transformedDataSrc,
    labelScale: {
      domain: {
        signal: `[${binSignal}.start, ${binSignal}.stop]`,
      },
      type: 'linear',
      bins: { signal: binSignal },
    },
    display,
  }, spec);
}

function convertToVegaChart(display: VegaDisplay): VegaSpecWithProps {
  const spec: VisualizationSpec = JSON.parse(display.spec);
  let vgSpec: VgSpec;
  if (!spec[VEGA_SCHEMA]) {
    spec[VEGA_SCHEMA] = VEGA_V5;
  }
  if (spec[VEGA_SCHEMA] === VEGA_LITE_V4) {
    vgSpec = vegaLite.compile(spec as VlSpec).spec;
  } else {
    vgSpec = spec as VgSpec;
  }
  return {
    spec: vgSpec,
    hasLegend: false,
    legendColumnName: '',
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
export const TS_DOMAIN_SIGNAL = 'ts_domain_value';
export const EXTERNAL_TS_DOMAIN_SIGNAL = 'external_ts_domain_value';
export const INTERNAL_TS_DOMAIN_SIGNAL = 'internal_ts_domain_value';
export const VALUE_DOMAIN_SIGNAL = 'y_domain';
// The minimum upper bound of the Y axis domain. If the y domain upper bound is
// less than this value, we set this as the upper bound.
export const DOMAIN_MIN_UPPER_VALUE = 1;

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
        fill: { value: theme.palette.foreground.grey3 },
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

function hydrateSpecWithTheme(spec: VgSpec, theme: Theme) {
  spec.background = theme.palette.background.six;
  spec.padding = theme.spacing(2);
  spec.config = {
    ...spec.config,
    style: {
      bar: {
        // binSpacing: 2,
        fill: theme.palette.graph.primary,
        stroke: null,
      },
      cell: {
        stroke: 'transparent',
      },
      arc: {
        fill: theme.palette.graph.primary,
      },
      area: {
        fill: theme.palette.graph.primary,
      },
      line: {
        stroke: theme.palette.graph.primary,
        strokeWidth: 1,
      },
      symbol: {
        shape: 'circle',
      },
      rect: {
        fill: theme.palette.graph.primary,
      },
      'group-title': {
        fontSize: 0,
      },
      'grouped-bar-label-title': {
        fill: theme.palette.foreground.one,
        fontSize: 12,
      },
      'grouped-bar-label-subtitle': {
        fill: theme.palette.foreground.one,
        fontSize: 10,
      },
      'bar-value-text': {
        font: 'Roboto',
        fontSize: 10,
        fill: theme.palette.foreground.one,
      },
    },
    axis: {
      labelColor: theme.palette.text.primary,
      labelFont: 'Roboto',
      labelFontSize: 14,
      labelPadding: theme.spacing(0.5),
      tickColor: theme.palette.foreground.grey4,
      tickSize: 10,
      tickWidth: 1,
      titleColor: theme.palette.text.primary,
      titleFont: 'Roboto',
      titleFontSize: 15,
      // titleFontWeight: theme.typography.fontWeightRegular,
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
      gridColor: theme.palette.foreground.grey4,
      gridWidth: 0.5,
    },
    axisBand: {
      grid: false,
    },
    group: {
      fill: theme.palette.foreground.grey5,
    },
    path: {
      stroke: theme.palette.graph.primary,
      strokeWidth: 0.5,
    },
    range: {
      category: theme.palette.graph.category,
      diverging: theme.palette.graph.diverging,
      heatmap: theme.palette.graph.heatmap,
    },
    shape: {
      stroke: theme.palette.graph.primary,
    },
  };
}

/* eslint-enable @typescript-eslint/no-use-before-define */
