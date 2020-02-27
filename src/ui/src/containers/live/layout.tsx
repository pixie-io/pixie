import { VisualizationSpec } from 'vega-embed';

export interface ChartPosition {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface Chart {
  vegaKey: string;
  description: string;
  position: ChartPosition;
}

export interface Layout {
  vegaSpec: VisualizationSpecMap;
  charts: Chart[];
}

export interface VisualizationSpecMap  { [key: string]: VisualizationSpec; }

// Tiles a grid with the VegaSpec keys.
function layoutDefaultGrid(vegaSpec: VisualizationSpecMap, gridWidth: number = 2): Chart[] {
  const charts: Chart[] = [];
  let curX: number = 0;
  let curY: number = 0;
  Object.keys(vegaSpec).forEach((key) => {
    // If we exceed the current width, move to the next row.
    if (curX >= gridWidth) {
      curX = 0;
      ++curY;
    }
    const pos: ChartPosition = { x: curX, y: curY, w: 1, h: 1 };
    // Create a chart with the default description.
    charts.push({ vegaKey: key, description: '', position: pos });
    // Move the next position to the right.
    ++curX;
  });
  return charts;
}

// buildLayout compiles a layout given the inputs. If the grid element
// is null, the vega configs are tiled evenly across the Grid.
export const buildLayout = (vegaSpecMap: VisualizationSpecMap, chartLayout?: Chart[]): Layout => {
  if (!chartLayout) {
    chartLayout = layoutDefaultGrid(vegaSpecMap);
  }
  return {
    vegaSpec: vegaSpecMap,
    charts: chartLayout,
  };
};
