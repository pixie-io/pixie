import { Theme } from '@material-ui/core/styles';
import * as _ from 'lodash';
import { Data, Mark, Signal, Spec as VgSpec } from 'vega';
import { VisualizationSpec } from 'vega-embed';
import { TopLevelSpec as VlSpec } from 'vega-lite';

import { DISPLAY_TYPE_KEY, WidgetDisplay } from './vis';

const BAR_CHART_TYPE = 'pixielabs.ai/pl.vispb.BarChart';
const VEGA_CHART_TYPE = 'pixielabs.ai/pl.vispb.VegaChart';
const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
const VEGA_LITE_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega-lite/';
const VEGA_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega/';
const VEGA_SCHEMA = '$schema';
const TIMESERIES_CHART_TYPE = 'pixielabs.ai/pl.vispb.TimeseriesChart';
const COLOR_SCALE = 'color';
const HOVER_LINE_COLOR = '#00dba6';
const HOVER_LINE_OPACITY = 0.3;
const HOVER_LINE_DASH = [6, 6];
const HOVER_LINE_WIDTH = 2;

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
      throw new Error('Unsupported display type: ' + display[DISPLAY_TYPE_KEY]);
  }
}

export function convertWidgetDisplayToVegaSpec(display: ChartDisplay, source: string, theme: Theme,
                                               vegaLiteModule): VisualizationSpec {
  const vegaLiteSpec = convertWidgetDisplayToVegaLiteSpec(display, source);
  const hydratedVegaLite = hydrateSpec(vegaLiteSpec, theme);
  const vegaSpec = vegaLiteModule.compile(hydratedVegaLite).spec;
  return addExtrasToVegaSpec(vegaSpec, display, source);
}

// Currently only supports a single input dataframe.
// TODO(nserrino): Add support for the multi-dataframe case.
function addSources(spec: VisualizationSpec, source: string): VisualizationSpec {
  // Vega takes the data field as an array, whereas Vega-Lite takes it as a single object.
  if (spec[VEGA_SCHEMA].includes(VEGA_SCHEMA_SUBSTRING)) {
    const vgspec = spec as VgSpec;
    return  {...vgspec, data: [...(vgspec.data || []), { name: source }]};
  }
  const vlspec = spec as VlSpec;
  return {...vlspec, data: { name: source }};
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
      y: { field: yField, type: 'quantitative'},
    },
    layer: [
      {mark},
    ],
  };
};

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
  return  {
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
      joinaggregate : [
        {
          field : 'time_',
          op : 'max',
          as : 'max_time',
        },
        {
          field : 'time_',
          op : 'min',
          as : 'min_time',
        },
      ],
    },
    {
      filter : 'datum.time_ > datum.min_time && datum.time_ < datum.max_time',
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
    spec = {...spec, title: display.title};
  }
  if (display.xAxis && display.xAxis.label) {
    spec = extendXEncoding(spec, {title: display.xAxis.label});
  }

  if (!display.timeseries) {
    throw new Error('TimeseriesChart must have one timeseries entry');
  }
  if (display.timeseries.length > 1) {
    throw new Error('More than one timeseries in TimeseriesChart not yet supported');
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

  if (!timeseries.value) {
    throw new Error('No value provided for TimeseriesChart timeseries');
  }

  const layers = [];
  layers.push(timeseriesDataLayer(timeseries.value, mark));
  if (display.yAxis && display.yAxis.label) {
    layers[0] = extendYEncoding(layers[0], {title: display.yAxis.label});
  }

  let colorField: string;
  if (timeseries.series) {
    colorField = timeseries.series;
  } else {
    // If there is no series provided, then we generate a series column,
    // by using the fold transform.
    // To avoid collisions, we generate a random name for the fields
    // that the fold transform creates.

    // create random alphanumeric strings of length 10.
    colorField = randStr(10);
    const valueField = randStr(10);
    spec = extendTransforms(spec, [
      {fold: [timeseries.value], as: [colorField, valueField]},
    ]);
  }
  layers[0] = extendColorEncoding(layers[0], {field: colorField, type: 'nominal', legend: null});

  if (timeseries.stackBySeries) {
    if (!timeseries.series) {
      throw new Error('stackBySeries is invalid for TimeseriesChart when series is not specified');
    }
    layers[0] = extendYEncoding(layers[0], {aggregate: 'sum', stack: 'zero'});
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
  spec = extendXEncoding(spec, {field: display.bar.label});
  spec = extendYEncoding(spec, {field: display.bar.value});

  if (display.bar.stackBy) {
    spec = extendColorEncoding(spec, {field: display.bar.stackBy, type: 'nominal'});
    spec = extendYEncoding(spec, {aggregate: 'sum'});
  }

  if (display.yAxis && display.yAxis.label) {
    spec = extendYEncoding(spec, {title: display.yAxis.label});
  }

  // Grouped bar charts need different formatting in the x axis and title.
  if (!display.bar.groupBy) {
    if (display.xAxis && display.xAxis.label) {
      spec = extendXEncoding(spec, {title: display.xAxis.label});
    }
    if (display.title) {
      spec = {...spec, title: display.title};
    }
    return spec;
  }

  if (display.title) {
    spec = {...spec, title: {text: display.title, anchor: 'middle'}};
  }

  let xlabel = `${display.bar.groupBy}, ${display.bar.label}`;
  if (display.xAxis && display.xAxis.label) {
    xlabel = display.xAxis.label;
  }
  const header = {titleOrient: 'bottom', labelOrient: 'bottom', title: xlabel};

  // Use the Column encoding header instead of an x axis title to avoid per-group repetition.
  spec = extendXEncoding(spec, {title: null});
  spec = extendColumnEncoding(spec, {field: display.bar.groupBy, type: 'nominal', header});

  return spec;
}

function convertToVegaChart(display: VegaDisplay, source: string): VisualizationSpec {
  const spec: VisualizationSpec = JSON.parse(display.spec);
  if (!spec[VEGA_SCHEMA]) {
    spec[VEGA_SCHEMA] = VEGA_LITE_V4;
  }
  return addSources(spec, source);
}

function addExtrasToVegaSpec(vegaSpec, display: ChartDisplay, source: string): VisualizationSpec {
  switch (display[DISPLAY_TYPE_KEY]) {
    case TIMESERIES_CHART_TYPE:
      return addExtrasForTimeseries(vegaSpec, display as TimeseriesDisplay, source);
    default:
      return vegaSpec;
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

function addExtrasForTimeseries(vegaSpec, display: TimeseriesDisplay, source: string): VisualizationSpec {
  const isStacked: boolean = display.timeseries[0].stackBySeries;
  const pivotField: string = extractPivotField(vegaSpec, display);
  if (!pivotField) {
    return vegaSpec;
  }
  const valueField: string = display.timeseries[0].value;
  const newSpec = addHoverHandlersToVgSpec(vegaSpec, source, isStacked, pivotField, valueField);
  return newSpec;
}

const HOVER_VORONOI = 'hover_voronoi_layer';
const TIME_FIELD = 'time_';
const HOVER_RULE = 'hover_rule_layer';
export const HOVER_SIGNAL = 'hover_value';
export const EXTERNAL_HOVER_SIGNAL = 'external_hover_value';
export const INTERNAL_HOVER_SIGNAL = 'internal_hover_value';
export const HOVER_PIVOT_TRANSFORM = 'hover_pivot_data';

function addHoverHandlersToVgSpec(vegaSpec: VgSpec, source: string, isStacked: boolean,
                                  pivotField: string, valueField: string): VgSpec {
  vegaSpec = addHoverDataToVgSpec(vegaSpec, source, pivotField, valueField);
  vegaSpec = addHoverSignalsToVgSpec(vegaSpec);
  vegaSpec = addHoverMarksToVgSpec(vegaSpec, isStacked);
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
            source: 'scope',
            type: 'mouseout',
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
        events: [{signal: EXTERNAL_HOVER_SIGNAL}, {signal: INTERNAL_HOVER_SIGNAL}],
        update: `${INTERNAL_HOVER_SIGNAL} || ${EXTERNAL_HOVER_SIGNAL}`,
      },
    ],
  });
  vegaSpec.signals.push(...signals);
  return vegaSpec;
}

function addHoverMarksToVgSpec(vegaSpec: VgSpec, isStacked: boolean): VgSpec {
  const marks: Mark[] = [];
  // Add mark for vertical line where cursor is.
  marks.push({
    name: HOVER_RULE,
    type: 'rule',
    style: ['rule'],
    interactive: true,
    from: {data: HOVER_PIVOT_TRANSFORM},
    encode: {
      enter: {
        stroke: {value: HOVER_LINE_COLOR},
        strokeDash: {value: HOVER_LINE_DASH},
        strokeWidth: {value: HOVER_LINE_WIDTH},
      },
      update: {
        opacity: [
          {
            test: `${HOVER_SIGNAL} && datum && (${HOVER_SIGNAL}["${TIME_FIELD}"] === datum["${TIME_FIELD}"])`,
            value: HOVER_LINE_OPACITY,
          },
          {value: 0},
        ],
        x: {scale: 'x', field: TIME_FIELD},
        y: {value: 0},
        y2: {signal: 'height'},
      },
    },
  });
  // Add mark for voronoi layer.
  marks.push({
    name: HOVER_VORONOI,
    type: 'path',
    interactive: true,
    from: {data: HOVER_RULE},
    encode: {
      update: {
        fill: {value: 'transparent'},
        strokeWidth: {value: 0.35},
        stroke: {value: 'transparent'},
        isVoronoi: {value: true},
        tooltip: {
          signal: `merge(datum.datum, {colorScale: "${COLOR_SCALE}", isStacked: ${isStacked}})`,
        },
      },
    },
    transform: [
      {
        type: 'voronoi',
        x: {expr: 'datum.datum.x || 0'},
        y: {expr: 'datum.datum.y || 0'},
        size: [{signal: 'width'}, {signal: 'height'}],
      },
    ],
  });
  vegaSpec.marks.push(...marks);
  return vegaSpec;
}

// We can get rid of this once we delete OldCanvas.
export function hydrateSpecOld(input, theme: Theme, tableName: string = 'output'): VisualizationSpec {
  return {
    ...input,
    ...specsFromTheme(theme),
    data: {name: tableName},
  };
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
