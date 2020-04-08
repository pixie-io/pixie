import {Spec as VgSpec} from 'vega';
import {VisualizationSpec} from 'vega-embed';
import {TopLevelSpec as VlSpec} from 'vega-lite';

const BAR_CHART_TYPE = 'pixielabs.ai/pl.vispb.BarChart';
const VEGA_CHART_TYPE = 'pixielabs.ai/pl.vispb.VegaChart';
const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
const VEGA_LITE_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega-lite/';
const VEGA_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega/';
const VEGA_SCHEMA = '$schema';
const TIMESERIES_CHART_TYPE = 'pixielabs.ai/pl.vispb.TimeseriesChart';
const TYPE_KEY = '@type';

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
  readonly stack_by_series?: boolean;
}

interface TimeseriesDisplay {
  readonly '@type': string;
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
  readonly timeseries: Timeseries[];
}

interface Bar {
  readonly value: string;
  readonly label: string;
  readonly stack_by?: string;
  readonly group_by?: string;
}

interface BarDisplay {
  readonly '@type': string;
  readonly title?: string;
  readonly xAxis?: XAxis;
  readonly yAxis?: YAxis;
  readonly bar: Bar;
}

interface VegaDisplay {
  readonly '@type': string;
  readonly spec: string;
}

export type ChartDisplay = TimeseriesDisplay | BarDisplay | VegaDisplay;

export function convertWidgetDisplayToVegaSpec(display: ChartDisplay, source: string): VisualizationSpec {
  switch (display[TYPE_KEY]) {
    case BAR_CHART_TYPE:
      return convertToBarChart(display as BarDisplay, source);
    case TIMESERIES_CHART_TYPE:
      return convertToTimeseriesChart(display as TimeseriesDisplay, source);
    case VEGA_CHART_TYPE:
      return convertToVegaChart(display as VegaDisplay, source);
    default:
      throw new Error('Unsupported display type: ' + display[TYPE_KEY]);
  }
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
    y: {
      type: 'quantitative',
    },
  },
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

function convertToTimeseriesChart(display: TimeseriesDisplay, source: string): VisualizationSpec {
  let spec = BASE_TIMESERIES_SPEC;

  if (display.title) {
    spec = {...spec, title: display.title};
  }
  if (display.xAxis && display.xAxis.label) {
    spec = extendXEncoding(spec, {title: display.xAxis.label});
  }
  if (display.yAxis && display.yAxis.label) {
    spec = extendYEncoding(spec, {title: display.yAxis.label});
  }

  if (!display.timeseries) {
    throw new Error('TimeseriesChart must have one timeseries entry');
  }
  if (display.timeseries.length > 1) {
    throw new Error('More than one timeseries in TimeseriesChart not yet supported');
  }

  const timeseries = display.timeseries[0];

  switch (timeseries.mode) {
    case 'MODE_POINT':
      spec = {...spec, mark: 'point'};
      break;
    case 'MODE_BAR':
      spec = {...spec, mark: 'bar'};
      break;
    case 'MODE_UNKNOWN':
    case 'MODE_LINE':
    default:
      spec = {...spec, mark: 'line'};
  }

  if (!timeseries.value) {
    throw new Error('No value provided for TimeseriesChart timeseries');
  }
  spec = extendYEncoding(spec, {field: timeseries.value});
  if (timeseries.series ) {
    spec = extendColorEncoding(spec, {field: timeseries.series, type: 'nominal'});
  }

  if (timeseries.stack_by_series) {
    if (!timeseries.series) {
      throw new Error('stack_by_series is invalid for TimeseriesChart when series is not specified');
    }
    spec = extendYEncoding(spec, {aggregate: 'sum', stack: 'zero'});
  }

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

  if (display.bar.stack_by) {
    spec = extendColorEncoding(spec, {field: display.bar.stack_by, type: 'nominal'});
    spec = extendYEncoding(spec, {aggregate: 'sum'});
  }

  if (display.yAxis && display.yAxis.label) {
    spec = extendYEncoding(spec, {title: display.yAxis.label});
  }

  // Grouped bar charts need different formatting in the x axis and title.
  if (!display.bar.group_by) {
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

  let xlabel = `${display.bar.group_by}, ${display.bar.label}`;
  if (display.xAxis && display.xAxis.label) {
    xlabel = display.xAxis.label;
  }
  const header = {titleOrient: 'bottom', labelOrient: 'bottom', title: xlabel};

  // Use the Column encoding header instead of an x axis title to avoid per-group repetition.
  spec = extendXEncoding(spec, {title: null});
  spec = extendColumnEncoding(spec, {field: display.bar.group_by, type: 'nominal', header});

  return spec;
}

function convertToVegaChart(display: VegaDisplay, source: string): VisualizationSpec {
  const spec: VisualizationSpec = JSON.parse(display.spec);
  if (!spec[VEGA_SCHEMA]) {
    spec[VEGA_SCHEMA] = VEGA_LITE_V4;
  }
  return addSources(spec, source);
}
