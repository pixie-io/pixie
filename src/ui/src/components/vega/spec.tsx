import {VisualizationSpec} from 'vega-embed';

import {Theme} from '@material-ui/core/styles';

export function hydrateSpecOld(input, theme: Theme, tableName: string = 'output'): VisualizationSpec {
  return {
    ...specsFromTheme(theme),
    ...input,
    ...BASE_SPECS,
    data: { name: tableName },
  };
}

export function hydrateSpec(input, theme: Theme): VisualizationSpec {
  return {
    ...specsFromTheme(theme),
    ...input,
    ...BASE_SPECS,
  };
}

export interface VisualizationSpecMap {
  [key: string]: VisualizationSpec;
}

export function parseSpecs(spec: string): VisualizationSpecMap {
  try {
    const vega = JSON.parse(spec);
    return vega as VisualizationSpecMap;
  } catch (e) {
    return null;
  }
}

function specsFromTheme(theme: Theme) {
  return {
    background: theme.palette.background.default,
  };
}

export const COLOR_SCALE = 'color';

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
      anchor: 'start',
      fontSize: 24,
      fontWeight: 600,
      offset: 20,
    },
  },
};

// This function is not ideal. But it will go away when we move convert-to-vega-spec.tsx to produce
// an actual vega spec instead of a vega-lite spec. Once we do that, both this logic and addTooltipsToSpec
// will move to convert-to-vega-spec.tsx
function isStacked(vegaLiteSpec): boolean {
  return vegaLiteSpec && vegaLiteSpec.layer && vegaLiteSpec.layer.some((layer) => {
    return layer.encoding && layer.encoding.y && layer.encoding.y.stack;
  });
}

/**
 * Add the tooltip spec to a Vega spec (not vega-lite spec).
 * Note that this currently will add a tooltip to every voronoi layer.
 */
export function addTooltipsToSpec(vegaSpec, vegaLiteSpec) {
  const marks = vegaSpec.marks;
  if (!marks) {
    return vegaSpec;
  }
  vegaSpec.marks = marks.map((mark) => {
    if (mark.type !== 'path'
      || !mark.interactive
      || !mark.encode
      || !mark.encode.update
      || !mark.encode.update.isVoronoi
      || !mark.encode.update.isVoronoi.value) {
      return mark;
    }
    return {
      ...mark,
      encode: {
        ...mark.encode,
        update: {
          ...mark.encode.update,
          tooltip: {
            signal: `merge(datum.datum, {colorScale: "${COLOR_SCALE}", isStacked: ${isStacked(vegaLiteSpec)}})`,
          },
        },
      },
    };
  });
  return vegaSpec;
}
