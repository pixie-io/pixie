import {VisualizationSpecMap} from 'components/vega/spec';

import {Vis, widgetResultName} from './vis';

export interface ChartPosition {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface Layout extends ChartPosition {
  i: string;
}

export const GRID_WIDTH = 12;
export const DEFAULT_HEIGHT = 3;

// Tiles a grid with the vis spec widgets.
function layoutDefaultGrid(visSpec: Vis): Vis {
  const newVis = {
    ...visSpec,
    widgets: [],
  };
  let curX: number = 0;
  let curY: number = 0;
  const elemWidth = GRID_WIDTH / 2;

  visSpec.widgets.forEach((widget) => {
    // If we exceed the current width, move to the next row.
    if (curX >= GRID_WIDTH) {
      curX = 0;
      curY += DEFAULT_HEIGHT;
    }
    newVis.widgets.push({
      ...widget,
      position: { x: curX, y: curY, w: elemWidth, h: DEFAULT_HEIGHT },
    });
    // Move the next position to the right.
    curX += elemWidth;
  });

  return newVis;
}

export function addLayout(visSpec: Vis): Vis {
  for (const widget of visSpec.widgets) {
    if (!widget.position) {
      return layoutDefaultGrid(visSpec);
    }
  }
  return visSpec;
}

export function toLayout(visSpec: Vis): Layout[] {
  return visSpec.widgets.map((widget, i) => {
    return {
      ...widget.position,
      i: widgetResultName(widget, i),
      x: widget.position.x || 0,
      y: widget.position.y || 0,
      minH: 2,
      minW: 2,
    };
  });
}

export function updatePositions(visSpec: Vis, layouts: ChartPosition[]): Vis {
  return {
    variables: visSpec.variables,
    widgets: layouts.map((layout, i) => {
      return {
        ...visSpec.widgets[i],
        position: {
          x: layout.x,
          y: layout.y,
          w: layout.w,
          h: layout.h,
        },
      };
    }),
  };
}
