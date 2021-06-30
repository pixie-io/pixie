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

export function getGridWidth(isMobile: boolean): number {
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
  for (let i = 0; i < visSpec?.widgets.length; ++i) {
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
export function toLayout(widgets: Widget[], isMobile: boolean, selectedWidget: string | null): Layout[] {
  if (widgets == null) { // Script has no vis spec.
    return [];
  }

  // Single-widget layout if a widget selector is specified.
  if (selectedWidget) {
    return [{
      i: selectedWidget,
      x: 0,
      y: 0,
      w: (isMobile ? MOBILE_GRID_WIDTH : DEFAULT_GRID_WIDTH) * 1.5,
      h: (isMobile ? MOBILE_WIDGET_HEIGHT : DEFAULT_WIDGET_HEIGHT) * 1.5,
    }];
  }

  const nonMobileLayout = widgets.map((widget, i) => ({
    ...widget.position,
    i: widgetName(widget, i),
    x: widget.position?.x || 0,
    y: widget.position?.y || 0,
    w: widget.position?.w || 0,
    h: widget.position?.h || 0,
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

function samePosition(oldPos: ChartPosition, newPos: ChartPosition): boolean {
  if (oldPos == null && newPos != null) {
    return false;
  }
  return oldPos === newPos
    || oldPos.x === newPos.x
    || oldPos.y === newPos.y
    || oldPos.w === newPos.w
    || oldPos.h === newPos.h;
}

export function updatePositions(visSpec: Vis, positions: ChartPosition[]): Vis {
  let different = false;
  const newVis = { variables: [...visSpec.variables], globalFuncs: [...visSpec.globalFuncs], widgets: [] };
  for (let i = 0; i < positions.length; i++) {
    const position = positions[i];
    const widget = visSpec.widgets[i];
    if (!samePosition(visSpec.widgets[i].position, position)) {
      different = true;
    }
    newVis.widgets.push({ ...widget, position: { ...position } });
  }
  // Only return a newVis if the widgets were changed, otherwise we want to preserve object identity.
  if (different) {
    return newVis;
  }
  return visSpec;
}

// Helper function to generate a default layout for tables without vis specs.
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
