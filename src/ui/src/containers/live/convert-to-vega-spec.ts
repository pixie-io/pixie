import * as _ from 'lodash';
import {Spec as VgSpec} from 'vega';
import {VisualizationSpec} from 'vega-embed';
import {EncodeEntry} from 'vega-typings';
import {TopLevelSpec as VlSpec} from 'vega-lite';

import {DISPLAY_TYPE_KEY, WidgetDisplay} from './vis';

const BAR_CHART_TYPE = 'pixielabs.ai/pl.vispb.BarChart';
const VEGA_CHART_TYPE = 'pixielabs.ai/pl.vispb.VegaChart';
const VEGA_LITE_V4 = 'https://vega.github.io/schema/vega-lite/v4.json';
const VEGA_LITE_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega-lite/';
const VEGA_SCHEMA_SUBSTRING = 'vega.github.io/schema/vega/';
const VEGA_SCHEMA = '$schema';
const TIMESERIES_CHART_TYPE = 'pixielabs.ai/pl.vispb.TimeseriesChart';


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

export function convertWidgetDisplayToVegaSpec(display: ChartDisplay, source: string): VisualizationSpec {
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

function timeseriesHoverLayer(colorField: string, yField: string, timeField: string) {
  return {
    transform: [{
      pivot: colorField,
      value: yField,
      groupby: [timeField],
    }],
    mark: 'rule',
    encoding: {
      opacity: {
        condition: {value: 0.3, selection: 'hover'},
        value: 0,
      },
    },
    selection: {
      hover: {
        type: 'single',
        fields: [timeField],
        nearest: true,
        on: 'mouseover',
        empty: 'none',
        clear: 'mouseout',
      },
    },
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

function convertToTimeseriesChart(display: TimeseriesDisplay, source: string): VisualizationSpec {
  let spec = BASE_TIMESERIES_SPEC;

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
  if (timeseries.series ) {
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
  // Add layer for voronoi and hover line.
  layers.push(timeseriesHoverLayer(colorField, timeseries.value, TIMESERIES_TIME_COLUMN));

  if (timeseries.stackBySeries) {
    if (!timeseries.series) {
      throw new Error('stackBySeries is invalid for TimeseriesChart when series is not specified');
    }
    layers[0] = extendYEncoding(layers[0], {aggregate: 'sum', stack: 'zero'});
  }

  spec = extendLayer(spec, layers);

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
