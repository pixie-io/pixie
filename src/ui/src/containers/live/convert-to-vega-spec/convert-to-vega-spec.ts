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
  Spec as VgSpec,
  expressionFunction,
} from 'vega';
import { vegaLite, VisualizationSpec } from 'vega-embed';
import { TopLevelSpec as VlSpec } from 'vega-lite';

import { getFormatFnMetadata, DataWithUnits } from 'app/containers/format-data/format-data';
import { DISPLAY_TYPE_KEY, WidgetDisplay } from 'app/containers/live/vis';
import { addPxTimeFormatExpression } from 'app/containers/live-widgets/vega/timeseries-axis';
import { Relation, SemanticType } from 'app/types/generated/vizierapi_pb';

import { BarDisplay, convertToBarChart } from './bar';
import {
  BAR_CHART_TYPE,
  FLAMEGRAPH_CHART_TYPE,
  HISTOGRAM_CHART_TYPE,
  GAUGE_CHART_TYPE,
  TIMESERIES_CHART_TYPE,
  VEGA_CHART_TYPE,
  VEGA_LITE_V4,
  VEGA_SCHEMA,
  VEGA_V5,
  VegaSpecWithProps,
  PIE_CHART_TYPE,
} from './common';
import {
  StacktraceFlameGraphDisplay,
  convertToStacktraceFlameGraph,
} from './flamegraph';
import { convertToGaugeChart, GaugeDisplay } from './gauge';
import { convertToHistogramChart, HistogramDisplay } from './histogram';
import { convertToPieChart, PieDisplay } from './pie';
import {
  convertToTimeseriesChart,
  TimeseriesDisplay,
} from './timeseries';

addPxTimeFormatExpression();

export const COLOR_SCALE = 'color';

// TODO(philkuz) A bit of a hack to get the column from the display,
// fix when you fix the heterogenous timeseries types fix.
export function getColumnFromDisplay(display: ChartDisplay): string {
  switch (display[DISPLAY_TYPE_KEY]) {
    case TIMESERIES_CHART_TYPE: {
      const ts = (display as TimeseriesDisplay);
      if (!ts.timeseries) {
        return '';
      }
      return ts.timeseries[0].value;
    }
    default:
      return '';
  }
}

interface VegaDisplay extends WidgetDisplay {
  readonly spec: string;
}

export function wrapFormatFn(fn: (data: number) => DataWithUnits) {
  return (val: number): string => {
    const fmt = fn(val);
    return `${fmt.val}${fmt.units}`;
  };
}

function registerVegaFormatFunctions() {
  Object.values(SemanticType).forEach((semType: SemanticType) => {
    const fnMetadata = getFormatFnMetadata(semType);
    if (fnMetadata) {
      expressionFunction(fnMetadata.name, wrapFormatFn(fnMetadata.formatFn));
    }
  });
}

registerVegaFormatFunctions();

export type ChartDisplay =
| TimeseriesDisplay
| BarDisplay
| PieDisplay
| VegaDisplay
| HistogramDisplay
| GaugeDisplay
| StacktraceFlameGraphDisplay;

function hydrateSpecWithTheme(spec: VgSpec, theme: Theme) {
  spec.padding = 16;
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
      labelPadding: 4,
      tickColor: theme.palette.foreground.grey4,
      tickSize: 10,
      tickWidth: 1,
      titleColor: theme.palette.text.primary,
      titleFont: 'Roboto',
      titleFontSize: 15,
      // titleFontWeight: theme.typography.fontWeightRegular,
      titlePadding: 24,
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
      tickSize: 4,
      gridColor: theme.palette.foreground.grey4,
      gridWidth: 0.5,
    },
    axisBand: {
      grid: false,
    },
    group: {
      fill: spec.config?.style?.group?.fill ?? theme.palette.foreground.grey5,
    },
    path: {
      stroke: theme.palette.graph.primary,
      strokeWidth: 0.5,
    },
    range: {
      category: theme.palette.graph.category,
      diverging: theme.palette.graph.diverging,
      heatmap: theme.palette.graph.heatmap,
      ramp: theme.palette.graph.ramp,
    },
    shape: {
      stroke: theme.palette.graph.primary,
    },
  };
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

function convertWidgetDisplayToSpecWithErrors(
  display: ChartDisplay,
  source: string,
  theme: Theme,
  relation?: Relation,
): VegaSpecWithProps {
  switch (display[DISPLAY_TYPE_KEY]) {
    case BAR_CHART_TYPE:
      return convertToBarChart(display as BarDisplay, source, relation);
    case PIE_CHART_TYPE:
      return convertToPieChart(display as PieDisplay, source, theme, relation);
    case HISTOGRAM_CHART_TYPE:
      return convertToHistogramChart(display as HistogramDisplay, source, relation);
    case TIMESERIES_CHART_TYPE:
      return convertToTimeseriesChart(display as TimeseriesDisplay, source, theme, relation);
    case GAUGE_CHART_TYPE:
      return convertToGaugeChart(
        display as GaugeDisplay,
        source,
        theme,
        relation,
      );
    case FLAMEGRAPH_CHART_TYPE:
      return convertToStacktraceFlameGraph(
        display as StacktraceFlameGraphDisplay,
        source,
        theme,
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
