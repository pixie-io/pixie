import { VisualizationSpecMap } from 'components/vega/spec';

import { Vis, Widget } from './vis';

export interface ChartPosition {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface Layout extends ChartPosition {
  i: string;
  minH?: number;
  minW?: number;
}

export const GRID_WIDTH = 12;
export const DEFAULT_HEIGHT = 3;

// Tiles a grid with the vis spec widgets.
export function layoutDefaultGrid<T>(widgets: T[], numCols = 2): Array<T & { position: ChartPosition }> {
  let curX: number = 0;
  let curY: number = 0;
  const elemWidth = GRID_WIDTH / numCols;

  return widgets.map((widget) => {
    // If we exceed the current width, move to the next row.
    if (curX >= GRID_WIDTH) {
      curX = 0;
      curY += DEFAULT_HEIGHT;
    }
    const newWidget = {
      ...widget,
      position: { x: curX, y: curY, w: elemWidth, h: DEFAULT_HEIGHT },
    };
    // Move the next position to the right.
    curX += elemWidth;
    return newWidget;
  });
}

export function addLayout(visSpec: Vis): Vis {
  for (const widget of visSpec.widgets) {
    if (!widget.position) {
      return {
        ...visSpec,
        widgets: layoutDefaultGrid(visSpec.widgets),
      };
    }
  }
  return visSpec;
}

export function toLayout(widget: Widget, widgetName: string): Layout {
  return {
    ...widget.position,
    i: widgetName,
    x: widget.position.x || 0,
    y: widget.position.y || 0,
    minH: 2,
    minW: 2,
  };
}

export function updatePositions(visSpec: Vis, layouts: ChartPosition[]): Vis {
  return {
    variables: visSpec.variables,
    globalFuncs: visSpec.globalFuncs,
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

// Helper function to generate a default layout for tables without vis specs.
// TODO(malthus): Refactor this whole file to do this better. We probably need to
// decouple the positioning of widgets with the vis spec for more flexible manipulation.
export function addTableLayout(tables: string[], layout: Layout[]): Layout[] {
  const layoutSet = new Set(layout.map((l) => l.i));
  if (tables.every((table) => layoutSet.has(table))) {
    return layout;
  }
  const numCols = Math.floor(Math.sqrt(tables.length));
  return layoutDefaultGrid(tables.map((table) => ({ i: table })), numCols)
    .map(({ i, position }) => ({ i, ...position }));
}
