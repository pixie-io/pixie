import {VisualizationSpec} from 'vega-embed';

import {Theme, useTheme} from '@material-ui/core/styles';

export function hydrateSpec(input, theme: Theme, tableName: string = 'output'): VisualizationSpec {
  return {
    ...specsFromTheme(theme),
    ...input,
    ...BASE_SPECS,
    data: { name: tableName },
  };
}

export function parseSpecs(spec: string): VisualizationSpec {
  try {
    const vega = JSON.parse(spec);
    return vega as VisualizationSpec;
  } catch (e) {
    return {};
  }
}

function specsFromTheme(theme: Theme) {
  return {
    background: theme.palette.background.default,
  };
}

const BASE_SPECS = {
  $schema: 'https://vega.github.io/schema/vega-lite/v4.json',
  width: 'container',
  height: 'container',
  config: {
    arc: {
      fill: '#39A8F5',
    },
    area: {
      fill: '#39A8F5',
    },
    axis: {
      domain: false,
      domainColor: 'red',
      grid: true,
      gridColor: '#343434',
      gridWidth: 0.5,
      labelColor: '#A6A8AE',
      labelFontSize: 10,
      titleColor: '#A6A8AE',
      tickColor: '#A6A8AE',
      tickSize: 10,
      titleFontSize: 12,
      titleFontWeigth: 1,
      titlePadding: 8,
      titleFont: 'Lato',
      labelPadding: 4,
      labelFont: 'Lato',
    },
    axisBand: {
      grid: false,
    },
    background: '#272822',
    group: {
      fill: '#f0f0f0',
    },
    legend: {
      labelColor: '#A6A8AE',
      labelFontSize: 11,
      labelFont: 'Lato',
      padding: 1,
      symbolSize: 100,
      fillOpacity: 1,
      titleColor: '#A6A8AE',
      titleFontSize: 12,
      titlePadding: 5,
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
        '#1f77b4',
        '#aec7e8',
        '#ff7f0e',
        '#ffbb78',
        '#2ca02c',
        '#98df8a',
        '#d62728',
        '#ff9896',
        '#9467bd',
        '#c5b0d5',
        '#8c564b',
        '#c49c94',
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
      anchor: 'start',
      fontSize: 24,
      fontWeight: 600,
      offset: 20,
    },
  },
};
