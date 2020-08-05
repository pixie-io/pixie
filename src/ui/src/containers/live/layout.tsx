import { Vis, Widget, widgetTableName } from './vis';

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

// default layout for non-mobile case.
// in both mobile and non-mobile, numbers are in grid coordinates.
// 2:1 width/height ratio for default layout.
const DEFAULT_GRID_WIDTH = 12;
const DEFAULT_NUM_COLS = 2;
const DEFAULT_WIDGET_HEIGHT = 3;

// default layout for mobile case.
// 3:2 width/height ratio for mobile layout.
const MOBILE_GRID_WIDTH = 4;
const MOBILE_NUM_COLS = 1;
const MOBILE_WIDGET_HEIGHT = 2;

export function getGridWidth(isMobile: boolean) {
  return isMobile ? MOBILE_GRID_WIDTH : DEFAULT_GRID_WIDTH;
}

// Tiles a grid in a default way based on the number of widgets, number of columns, etc.
function widgetPositions(numWidgets: number, gridWidth: number, numCols: number,
  elemHeight: number): Array<ChartPosition> {
  let curX = 0;
  let curY = 0;
  const elemWidth = gridWidth / numCols;

  return [...Array(numWidgets)].map(() => {
    // If we exceed the current width, move to the next row.
    if (curX >= gridWidth) {
      curX = 0;
      curY += elemHeight;
    }
    const position = {
      x: curX, y: curY, w: elemWidth, h: elemHeight,
    };
    // Move the next position to the right.
    curX += elemWidth;
    return position;
  });
}

function defaultWidgetPositions(numWidgets: number) {
  return widgetPositions(numWidgets, DEFAULT_GRID_WIDTH, DEFAULT_NUM_COLS, DEFAULT_WIDGET_HEIGHT);
}

function mobileWidgetPositions(numWidgets: number) {
  return widgetPositions(numWidgets, MOBILE_GRID_WIDTH, MOBILE_NUM_COLS, MOBILE_WIDGET_HEIGHT);
}

// addLayout is only called in the non-mobile case.
export function addLayout(visSpec: Vis): Vis {
  for (let i = 0; i < visSpec.widgets.length; ++i) {
    const widget = visSpec.widgets[i];
    if (!widget.position) {
      const positions = defaultWidgetPositions(visSpec.widgets.length);
      return {
        ...visSpec,
        widgets: visSpec.widgets.map((curWidget, j) => ({
          ...curWidget,
          position: positions[j],
        })),
      };
    }
  }
  return visSpec;
}

function widgetName(widget: Widget, widgetIndex: number): string {
  const tableName = widgetTableName(widget, widgetIndex);
  return widget.name || `${tableName}_${widgetIndex}`;
}

// Generates the layout of a Live View, with mobile-specific layout that follow the overall
// order of the vis spec positions but tiles it differently.
export function toLayout(widgets: Widget[], isMobile: boolean): Layout[] {
  const nonMobileLayout = widgets.map((widget, i) => ({
    ...widget.position,
    i: widgetName(widget, i),
    x: widget.position?.x || 0,
    y: widget.position?.y || 0,
    minH: 2,
    minW: 2,
  }));

  if (!isMobile) {
    return nonMobileLayout;
  }

  // Find out the row-major order in which the widgets would be displayed in the non-mobile view,
  // and match it for the mobile view.
  nonMobileLayout.sort((a: Layout, b: Layout) => {
    if (a.y < b.y) {
      return -1;
    }
    if (a.y > b.y) {
      return 1;
    }
    if (a.x < b.x) {
      return -1;
    }
    if (a.x > b.x) {
      return 1;
    }
    return 0;
  });
  const widgetOrderMap = new Map(nonMobileLayout.map((layout, idx) => [layout.i, idx]));

  const positions = mobileWidgetPositions(widgets.length);
  return widgets.map((widget, idx) => {
    // Use widgetName as i in nonMobileLayout above.
    const name = widgetName(widget, idx);
    return {
      ...positions[widgetOrderMap.get(name)],
      i: name,
    };
  });
}

export function updatePositions(visSpec: Vis, positions: ChartPosition[]): Vis {
  return {
    variables: visSpec.variables,
    globalFuncs: visSpec.globalFuncs,
    widgets: positions.map((position, i) => ({
      ...visSpec.widgets[i],
      position: {
        x: position.x,
        y: position.y,
        w: position.w,
        h: position.h,
      },
    })),
  };
}

// Helper function to generate a default layout for tables without vis specs.
// TODO(malthus): Refactor this whole file to do this better. We probably need to
// decouple the positioning of widgets with the vis spec for more flexible manipulation.
export function addTableLayout(tables: string[], layout: Layout[], isMobile: boolean, height: number): {
  layout: Layout[];
  numCols: number;
  rowHeight: number;
} {
  if (tables.length === 0) {
    return {
      layout: [],
      numCols: 0,
      rowHeight: 0,
    };
  }
  const numCols = isMobile ? 1 : Math.floor(Math.sqrt(tables.length));
  const numRows = Math.floor(tables.length / numCols);
  const rowHeight = Math.floor(height / numRows);
  const layoutSet = new Set(layout.map((l) => l.i));
  if (tables.every((table) => layoutSet.has(table))) {
    return { numCols, rowHeight, layout };
  }
  const positions = widgetPositions(tables.length, numCols, numCols, 1);
  return {
    layout: tables.map((table, idx) => ({
      ...positions[idx],
      i: table,
    })),
    numCols,
    rowHeight,
  };
}
