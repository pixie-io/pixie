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

import { COMMON_THEME } from 'app/components';
import { WidgetDisplay } from 'app/containers/live/vis';

import {
  addAutosize,
  addDataSource,
  addMark,
  addScale,
  addSignal,
  addWidthHeightSignals,
  BASE_SPEC,
  TRANSFORMED_DATA_SOURCE_NAME,
  VegaSpecWithProps,
} from './common';

const OVERLAY_COLOR = '#121212';
const OVERLAY_ALPHA = 0.04;
const OVERLAY_LEVELS = 1;

export const SHIFT_CLICK_FLAMEGRAPH_SIGNAL = 'shift_click_flamegraph';

export interface StacktraceFlameGraphDisplay extends WidgetDisplay {
  readonly stacktraceColumn: string;
  readonly countColumn: string;
  readonly percentageColumn?: string;
  readonly namespaceColumn?: string;
  readonly podColumn?: string;
  readonly containerColumn?: string;
  readonly pidColumn?: string;
  readonly nodeColumn?: string;
  readonly percentageLabel?: string;
}

function hexToRgb(hex: string) {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  if (!result) {
    return null;
  }
  return {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16),
  };
}

function generateColorScale(baseColor: string, overlayColor: string, overlayAlpha: number, levels: number) {
  const scale = [];
  const baseRGB = hexToRgb(baseColor);
  const overlayRGB = hexToRgb(overlayColor);
  let currLevel = 0;
  while (currLevel < levels) {
    const r = Math.round(baseRGB.r + (overlayRGB.r - baseRGB.r) * (overlayAlpha) * currLevel);
    const g = Math.round(baseRGB.g + (overlayRGB.g - baseRGB.g) * (overlayAlpha) * currLevel);
    const b = Math.round(baseRGB.b + (overlayRGB.b - baseRGB.b) * (overlayAlpha) * currLevel);

    scale.push(`rgb(${r},${g},${b})`);
    currLevel++;
  }

  return scale;
}

export function convertToStacktraceFlameGraph(
  display: StacktraceFlameGraphDisplay,
  source: string, theme: Theme): VegaSpecWithProps {
  // Size of font for the labels on each stacktrace.
  const STACKTRACE_LABEL_PX = 16;
  // Height of each stacktrace rectangle.
  const RECTANGLE_HEIGHT_PX = 32;

  // Proportion of the view height reserved for the minimap.
  const MINIMAP_HEIGHT_PERCENT = 0.1;

  // Any traces with less samples than this will be filtered out of the view.
  const MIN_SAMPLE_COUNT = 5;

  // Height of rectangle separating the minimap and the main view.
  const SEPARATOR_HEIGHT = 15;

  const MINIMAP_GREY_OUT_COLOR = 'gray';
  const SLIDER_COLOR = 'white';
  const SLIDER_NORMAL_WIDTH = 2;
  const SLIDER_HOVER_WIDTH = 8;
  const SLIDER_HITBOX_WIDTH = 14;

  // Colors for the stacktrace rectangles, depending on category of the trace.
  const KERNEL_FILL_COLOR = theme.palette.graph.flamegraph.kernel;
  const JAVA_FILL_COLOR = theme.palette.graph.flamegraph.java;
  const APP_FILL_COLOR = theme.palette.graph.flamegraph.app;
  const K8S_FILL_COLOR = theme.palette.graph.flamegraph.k8s;

  if (!display.stacktraceColumn) {
    throw new Error('StackTraceFlamegraph must have an entry for property stacktraceColumn');
  }
  if (!display.countColumn) {
    throw new Error('StackTraceFlamegraph must have an entry for property countColumn');
  }

  const spec = { ...BASE_SPEC, style: 'cell' };
  addAutosize(spec);
  addWidthHeightSignals(spec);
  addSignal(spec, {
    name: 'main_width',
    update: 'width',
  });
  addSignal(spec, {
    name: 'main_height',
    update: `height * ${1.0 - MINIMAP_HEIGHT_PERCENT}`,
  });
  addSignal(spec, {
    name: 'minimap_width',
    update: 'width',
  });
  addSignal(spec, {
    name: 'minimap_height',
    update: `height * ${MINIMAP_HEIGHT_PERCENT}`,
  });

  // Add data and transforms.
  const baseDataSrc = addDataSource(spec, { name: source });
  addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME,
    source: baseDataSrc.name,
    transform: [
      // Tranform the data into a hierarchical tree structure that can be consumed by Vega.
      {
        type: 'stratify',
        key: 'fullPath',
        parentKey: 'parent',
      },
      // Generates the layout for an adjacency diagram.
      {
        type: 'partition',
        field: 'weight',
        sort: { field: 'count' },
        size: [{ signal: 'width' }, { signal: 'height' }],
      },
      {
        type: 'filter',
        expr: `datum.count > ${MIN_SAMPLE_COUNT}`,
      },
      // Sets y values based on a fixed height rectangle.
      {
        type: 'formula',
        expr: `split(datum.fullPath, ";").length * (${RECTANGLE_HEIGHT_PX})`,
        as: 'y1',
      },
      {
        type: 'formula',
        expr: `(split(datum.fullPath, ";").length - 1) * (${RECTANGLE_HEIGHT_PX})`,
        as: 'y0',
      },
      // Flips the y-axis, as the partition transform actually creates an icicle chart.
      {
        type: 'formula',
        expr: '-datum.y0 + height',
        as: 'y0',
      },
      {
        type: 'formula',
        expr: '-datum.y1 + height',
        as: 'y1',
      },
      // Truncate name based on width and height of rect. These are just estimates on font size widths/heights, since
      // Vega's "limit" field for text marks is a static number.
      {
        type: 'formula',
        as: 'displayedName',
        expr: `(datum.y0 - datum.y1) > ${STACKTRACE_LABEL_PX} && (datum.x1 - datum.x0) >
          ${STACKTRACE_LABEL_PX + 168} ?
          truncate(datum.name, 1.5 * (datum.x1 - datum.x0)/(${STACKTRACE_LABEL_PX}) - 1) : ""`,
      },
      {
        type: 'extent',
        field: 'y0',
        signal: 'y_extent',
      },
    ],
  });

  addSignal(spec, {
    name: 'unit',
    value: {},
    on: [
      { events: 'mousemove', update: 'isTuple(group()) ? group(): unit' },
    ],
  });

  const minimapGroup = addMark(spec, {
    type: 'group',
    name: 'minimap_group',
    style: 'cell',
    encode: {
      enter: {
        x: { value: 0 },
        clip: { value: true },
        stroke: { value: 'gray' },
        strokeForeground: { value: true },
      },
      update: {
        width: { signal: 'minimap_width' },
        height: { signal: 'minimap_height' },
      },
    },
  });

  addMark(spec, {
    type: 'rect',
    name: 'separator',
    encode: {
      update: {
        x: { value: 0 },
        x2: { signal: 'main_width' },
        y: { signal: 'minimap_height' },
        y2: { signal: `minimap_height + ${SEPARATOR_HEIGHT}` },
        fill: { value: theme.palette.background.four },
      },
    },
  });
  const mainGroup = addMark(spec, {
    type: 'group',
    name: 'stacktrace_group',
    style: 'cell',
    encode: {
      enter: {
        clip: { value: true },
        stroke: { value: 'gray' },
        strokeForeground: { value: true },
      },
      update: {
        width: { signal: 'main_width' },
        height: { signal: 'main_height' },
        y: { signal: `minimap_height + ${SEPARATOR_HEIGHT}` },
      },
    },
  });

  // Add rectangles for each stacktrace.
  addMark(mainGroup, {
    type: 'rect',
    name: 'stacktrace_rect',
    from: { data: TRANSFORMED_DATA_SOURCE_NAME },
    encode: {
      enter: {
        fill: { scale: { datum: 'color' }, field: 'name' },
        tooltip: {
          signal: `datum.fullPath !== "all" && (datum.percentage ? {"title": datum.name, "Samples": datum.count,
            "${display.percentageLabel || 'Percentage'}": format(datum.percentage, ".2f") + "%"} :
            {"title": datum.name, "Samples": datum.count})`,
        },
      },
      update: {
        x: { scale: 'x', field: 'x0' },
        x2: { scale: 'x', field: 'x1' },
        y: { scale: 'y', field: 'y0' },
        y2: { scale: 'y', field: 'y1' },
        strokeWidth: { value: 1 },
        stroke: { value: theme.palette.background.four },
        zindex: { value: 0 },
      },
      hover: {
        stroke: { value: theme.palette.common.white },
        strokeWidth: { value: 2.4 },
        zindex: { value: 1 },
      },
      exit: {},
    },
  });

  // Add labels to the rects.
  addMark(mainGroup, {
    type: 'text',
    interactive: false,
    from: { data: TRANSFORMED_DATA_SOURCE_NAME },
    encode: {
      enter: {
        baseline: { value: 'middle' },
        fontSize: { value: STACKTRACE_LABEL_PX },
        dx: { value: 8 },
        font: { value: COMMON_THEME.typography.monospace.fontFamily },
        fontWeight: { value: 400 },
        fill: { value: theme.palette.background.three },
      },
      update: {
        x: { scale: 'x', field: 'x0' },
        y: { scale: 'y', field: 'y0' },
        dy: { value: -(RECTANGLE_HEIGHT_PX / 2) },
        text: {
          signal: `(scale("x", datum.x1) - scale("x", datum.x0)) >
            ${STACKTRACE_LABEL_PX + 8} ? truncate(datum.name,
            1.5 * (scale("x", datum.x1) - scale("x", datum.x0)) /
            ${STACKTRACE_LABEL_PX} - 1) : ""`,
        },
      },
    },
  });

  // Add signals for controlling pan/zoom.
  addSignal(spec, {
    name: 'grid_x',
    init: '[0, main_width]',
    on: [
      // Handle panning on main view.
      {
        events: { signal: 'grid_translate_delta' },
        update: `clampRange(panLinear(grid_translate_anchor.extent_x, -grid_translate_delta.x / main_width),
          0, main_width)`,
      },
      // handle scroll wheel zooming.
      {
        events: { signal: 'grid_zoom_delta' },
        update: `((grid_x[1] - grid_x[0]) > 1 || grid_zoom_delta > 1) ?
        clampRange(zoomLinear(domain("x"), grid_zoom_anchor.x, grid_zoom_delta), 0, main_width) : grid_x`,
      },
      // Handle ctrl+click to zoom in.
      {
        events: {
          source: 'scope',
          type: 'click',
          markname: 'stacktrace_rect',
          filter: ['event.ctrlKey || event.metaKey', 'event.button === 0'],
        },
        update: `clampRange(
          [datum.x0 - ((datum.x1 - datum.x0) / 2), datum.x1 + ((datum.x1 - datum.x0) / 2)], 0, main_width)`,
      },
      // Handle click + drag on minimap to select new section.
      {
        events: [
          { signal: 'minimap_anchor' },
          { signal: 'minimap_second_point' },
        ],
        update: `(minimap_anchor.x && minimap_second_point.x && minimap_slider_anchor.invalid) ?
          [min(minimap_anchor.x, minimap_second_point.x), max(minimap_anchor.x, minimap_second_point.x)] : grid_x`,
      },
      // Handle click on selection and drag to pan in minimap.
      {
        events: { signal: 'minimap_translate_delta' },
        update: `(!minimap_anchor.x) ?
          clampRange([
            minimap_translate_anchor.extent_x[0] + minimap_translate_delta.x,
            minimap_translate_anchor.extent_x[1] + minimap_translate_delta.x], 0, minimap_width) : grid_x`,
      },
      // Handle click + drag on left/right sliders in the minimap.
      {
        events: {
          source: 'window',
          type: 'mousemove',
          consume: true,
          between: [
            {
              source: 'scope',
              type: 'mousedown',
              filter: `event.item && event.item.mark &&
                (event.item.mark.name === "left_slider_hitbox" || event.item.mark.name === "right_slider_hitbox")`,
            },
            {
              source: 'window',
              type: 'mouseup',
            },
          ],
        },
        update: `(minimap_slider_anchor.left) ? [min(x(group()), grid_x[1] - 1), grid_x[1]] :
          [grid_x[0], max(x(group()), grid_x[0]+1)]`,
      },
      {
        events: {
          source: 'view',
          type: 'dblclick',
        },
        update: '[0, main_width]',
      },
    ],
  });

  addSignal(spec, {
    name: 'grid_y',
    update: '[y_extent[1] - main_height, y_extent[1]]',
    on: [
      {
        events: [{ source: 'view', type: 'dblclick' }],
        update: 'null',
      },
      {
        events: { signal: 'grid_translate_delta' },
        update: `clampRange(panLinear(grid_translate_anchor.extent_y, -grid_translate_delta.y / main_height),
          y_extent[0], y_extent[1])`,
      },
      {
        events: {
          source: 'view',
          type: 'dblclick',
        },
        update: '[y_extent[1] - main_height, y_extent[1]]',
      },
    ],
  });
  addSignal(spec, {
    name: 'grid_translate_anchor',
    value: {},
    on: [
      {
        events: [{
          source: 'scope',
          type: 'mousedown',
          filter: 'group() && group().mark && group().mark.name === "stacktrace_group"',
        }],
        update: '{x: event.x, y: event.y, extent_x: domain("x"), extent_y: domain("y")}',
      },
    ],
  });
  addSignal(spec, {
    name: 'grid_translate_delta',
    value: {},
    on: [
      {
        events: [
          {
            source: 'window',
            type: 'mousemove',
            consume: true,
            between: [
              {
                source: 'scope',
                type: 'mousedown',
                filter: 'group() && group().mark && group().mark.name === "stacktrace_group"',
              },
              { source: 'window', type: 'mouseup' },
            ],
          },
        ],
        update: '{x: grid_translate_anchor.x - event.x, y: grid_translate_anchor.y - event.y}',
      },
    ],
  });
  addSignal(spec, {
    name: 'grid_zoom_anchor',
    on: [
      {
        events: [{
          source: 'scope',
          type: 'wheel',
          consume: true,
          // Don't consume scroll events that were meant for a parent element.
          between: [
            { source: 'view', type: 'mouseup' },
            { source: 'window', type: 'mousedown' },
          ],
        }],
        update: '{x: invert("x", x(unit)), y: invert("y", y(unit))}',
      },
    ],
  });
  addSignal(spec, {
    name: 'grid_zoom_delta',
    on: [
      {
        events: [{
          source: 'scope',
          type: 'wheel',
          consume: true,
          // Don't consume scroll events that were meant for a parent element.
          between: [
            { source: 'view', type: 'mouseup' },
            { source: 'window', type: 'mousedown' },
          ],
        }],
        force: true,
        update: 'pow(1.001, event.deltaY * pow(16, event.deltaMode))',
      },
    ],
  });

  addSignal(spec, {
    name: 'minimap_anchor',
    value: {},
    on: [
      {
        events: {
          source: 'scope',
          type: 'mousedown',
          filter: 'group() && group().mark && group().mark.name === "minimap_group"',
        },
        update: `(x(group()) < grid_x[0] || grid_x[1] < x(group())) ?
          {x: clamp(x(group()), 0, minimap_width)} : minimap_anchor`,
      },
      {
        events: { source: 'window', type: 'mouseup' },
        update: '{}',
      },
    ],
  });

  addSignal(spec, {
    name: 'minimap_second_point',
    value: {},
    on: [
      {
        events: {
          source: 'window',
          type: 'mousemove',
          consume: true,
          between: [
            {
              source: 'scope',
              type: 'mousedown',
              filter: 'group() && group().mark && group().mark.name === "minimap_group"',
            },
            { source: 'window', type: 'mouseup' },
          ],
        },
        update: '{x: clamp(x(group()), 0, minimap_width)}',
      },
      {
        events: {
          source: 'scope',
          type: 'mousedown',
          filter: 'group() && group().mark && group().mark.name === "minimap_group"',
        },
        update: `(x(group()) < grid_x[0] || grid_x[1] < x(group())) ?
          {x: clamp(x(group()), 0, minimap_width)} : minimap_anchor`,
      },
      {
        events: {
          source: 'window',
          type: 'mouseup',
        },
        update: '{}',
      },
    ],
  });

  addSignal(spec, {
    name: 'minimap_translate_anchor',
    value: {},
    on: [
      {
        events: {
          source: 'scope',
          type: 'mousedown',
          filter: 'group() && group().mark && group().mark.name === "minimap_group"',
        },
        update: '{x: event.x, extent_x: domain("x")}',
      },
    ],
  });

  addSignal(spec, {
    name: 'minimap_translate_delta',
    value: {},
    on: [
      {
        events: {
          source: 'window',
          type: 'mousemove',
          consume: true,
          between: [
            {
              source: 'scope',
              type: 'mousedown',
              filter: 'group() && group().mark && group().mark.name === "minimap_group"',
            },
            {
              source: 'window',
              type: 'mouseup',
            },
          ],
        },
        update: '{x: event.x - minimap_translate_anchor.x}',
      },
    ],
  });

  addSignal(spec, {
    name: 'minimap_slider_anchor',
    value: {},
    on: [
      {
        events: {
          source: 'scope',
          type: 'mousedown',
          filter: `event.item && event.item.mark &&
            (event.item.mark.name === "left_slider_hitbox" || event.item.mark.name === "right_slider_hitbox")`,
        },
        update: '{left: event.item.mark.name === "left_slider_hitbox"}',
      },
      {
        events: {
          source: 'scope',
          type: 'mouseup',
        },
        update: '{invalid: true}',
      },
    ],
  });

  addSignal(spec, {
    name: SHIFT_CLICK_FLAMEGRAPH_SIGNAL,
    on: [
      {
        events: {
          source: 'scope',
          type: 'click',
          markname: 'stacktrace_rect',
          filter: ['event.shiftKey'],
        },
        update: '{path: datum.fullPath, symbol: datum.name, x: event.x, y: event.y}',
      },
    ],
  });

  addMark(minimapGroup, {
    type: 'rect',
    name: 'minimap_rect',
    from: { data: TRANSFORMED_DATA_SOURCE_NAME },
    encode: {
      enter: {
        fill: { scale: { datum: 'color' }, field: 'name' },
      },
      update: {
        x: { scale: 'x_minimap', field: 'x0' },
        x2: { scale: 'x_minimap', field: 'x1' },
        y: { scale: 'y_minimap', field: 'y0' },
        y2: { scale: 'y_minimap', field: 'y1' },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'grey_out_left',
    encode: {
      enter: { fill: { value: MINIMAP_GREY_OUT_COLOR }, fillOpacity: { value: 0.7 } },
      update: {
        x: {
          value: 0,
        },
        x2: {
          scale: 'x_minimap',
          signal: '(grid_x) ? grid_x[0] : 0',
        },
        y: {
          value: 0,
        },
        y2: {
          signal: 'minimap_height',
        },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'grey_out_right',
    encode: {
      enter: { fill: { value: MINIMAP_GREY_OUT_COLOR }, fillOpacity: { value: 0.7 } },
      update: {
        x: {
          scale: 'x_minimap',
          signal: '(grid_x) ? grid_x[1] : minimap_width',
        },
        x2: {
          signal: 'minimap_width',
        },
        y: {
          value: 0,
        },
        y2: {
          signal: 'minimap_height',
        },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'left_slider',
    encode: {
      enter: {
        fill: { value: SLIDER_COLOR },
      },
      update: {
        x: {
          scale: 'x_minimap',
          signal: '(grid_x) ? grid_x[0] - (left_slider_size / 2) : 0',
        },
        width: { signal: 'left_slider_size' },
        y: { value: 0 },
        y2: { signal: 'minimap_height' },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'right_slider',
    encode: {
      enter: {
        fill: { value: SLIDER_COLOR },
      },
      update: {
        x: {
          scale: 'x_minimap',
          signal: '(grid_x) ? grid_x[1] - (right_slider_size / 2) : 0',
        },
        width: { signal: 'right_slider_size' },
        y: { value: 0 },
        y2: { signal: 'minimap_height' },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'left_slider_hitbox',
    encode: {
      enter: {
        fill: { value: 'transparent' },
      },
      update: {
        x: {
          scale: 'x_minimap',
          signal: `(grid_x) ? grid_x[0] - ${SLIDER_HITBOX_WIDTH / 2} : 0`,
        },
        width: { value: SLIDER_HITBOX_WIDTH },
        y: { value: 0 },
        y2: { signal: 'minimap_height' },
      },
    },
  });
  addMark(minimapGroup, {
    type: 'rect',
    name: 'right_slider_hitbox',
    encode: {
      enter: {
        fill: { value: 'transparent' },
      },
      update: {
        x: {
          scale: 'x_minimap',
          signal: `(grid_x) ? grid_x[1] - ${SLIDER_HITBOX_WIDTH / 2} : 0`,
        },
        width: { value: SLIDER_HITBOX_WIDTH },
        y: { value: 0 },
        y2: { signal: 'minimap_height' },
      },
    },
  });
  addSignal(spec, {
    name: 'left_slider_size',
    value: SLIDER_NORMAL_WIDTH,
    on: [
      {
        events: {
          source: 'scope',
          type: 'mouseover',
          markname: 'left_slider_hitbox',
        },
        update: `${SLIDER_HOVER_WIDTH}`,
      },
      {
        events: {
          source: 'scope',
          type: 'mouseout',
          markname: 'left_slider_hitbox',
        },
        update: `${SLIDER_NORMAL_WIDTH}`,
      },
      {
        events: {
          source: 'window',
          type: 'mousemove',
          between: [
            {
              source: 'scope',
              type: 'mousedown',
              filter: 'event.item && event.item.mark && event.item.mark.name === "left_slider_hitbox"',
            },
            {
              source: 'window',
              type: 'mouseup',
            },
          ],
        },
        update: `${SLIDER_HOVER_WIDTH}`,
      },
      {
        events: { source: 'window', type: 'mouseup' },
        update: `${SLIDER_NORMAL_WIDTH}`,
      },
    ],
  });
  addSignal(spec, {
    name: 'right_slider_size',
    value: SLIDER_NORMAL_WIDTH,
    on: [
      {
        events: {
          source: 'scope',
          type: 'mouseover',
          markname: 'right_slider_hitbox',
        },
        update: `${SLIDER_HOVER_WIDTH}`,
      },
      {
        events: {
          source: 'scope',
          type: 'mouseout',
          markname: 'right_slider_hitbox',
        },
        update: `${SLIDER_NORMAL_WIDTH}`,
      },
      {
        events: {
          source: 'window',
          type: 'mousemove',
          between: [
            {
              source: 'scope',
              type: 'mousedown',
              filter: 'event.item && event.item.mark && event.item.mark.name === "right_slider_hitbox"',
            },
            {
              source: 'window',
              type: 'mouseup',
            },
          ],
        },
        update: `${SLIDER_HOVER_WIDTH}`,
      },
      {
        events: { source: 'window', type: 'mouseup' },
        update: `${SLIDER_NORMAL_WIDTH}`,
      },
    ],
  });

  addScale(spec, {
    name: 'x',
    type: 'linear',
    domain: [0, { signal: 'main_width' }],
    domainRaw: { signal: 'grid_x' },
    range: [0, { signal: 'main_width' }],
    zero: false,
  });
  addScale(spec, {
    name: 'y',
    type: 'linear',
    domain: [{ signal: 'y_extent[1] - main_height' }, { signal: 'y_extent[1]' }],
    domainRaw: { signal: 'grid_y' },
    range: [0, { signal: 'main_height' }],
    zero: false,
  });

  addScale(spec, {
    name: 'x_minimap',
    type: 'linear',
    domain: [0, { signal: 'minimap_width' }],
    range: [0, { signal: 'minimap_width' }],
    zero: false,
  });
  addScale(spec, {
    name: 'y_minimap',
    type: 'linear',
    domain: [{ signal: 'y_extent[0]' }, { signal: 'y_extent[1]' }],
    range: [0, { signal: 'minimap_height' }],
    zero: false,
  });

  // Color the rectangles based on type, so each stacktrace is a different color.
  addScale(spec, {
    name: 'kernel',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(KERNEL_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });
  addScale(spec, {
    name: 'java',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(JAVA_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });
  addScale(spec, {
    name: 'c',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(APP_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });
  addScale(spec, {
    name: 'other',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(APP_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });
  addScale(spec, {
    name: 'go',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(APP_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });
  addScale(spec, {
    name: 'k8s',
    type: 'ordinal',
    domain: { data: TRANSFORMED_DATA_SOURCE_NAME, field: 'name' },
    range: generateColorScale(K8S_FILL_COLOR, OVERLAY_COLOR, OVERLAY_ALPHA, OVERLAY_LEVELS),
  });

  const preprocess = (data: Array<Record<string, any>>): Array<Record<string, any>> => {
    const nodeMap = {
      all: {
        fullPath: 'all',
        name: 'all',
        weight: 0,
        count: 0,
        parent: null,
        color: 'k8s',
      },
    };

    for (const n of data) {
      let scopedStacktrace = n[display.stacktraceColumn];
      if (display.pidColumn) {
        scopedStacktrace = `pid: ${n[display.pidColumn] || 'UNKNOWN'}(k8s);${scopedStacktrace}`;
      }

      if (display.containerColumn) {
        scopedStacktrace = `container: ${n[display.containerColumn] || 'UNKNOWN'}(k8s);${scopedStacktrace}`;
      }

      if (display.podColumn) {
        // Remove namespace from podColumn, if any.
        let podCol = n[display.podColumn] || 'UNKNOWN';
        const nsIndex = podCol.indexOf('/');
        if (nsIndex !== -1) {
          podCol = podCol.substring(nsIndex + 1);
        }
        scopedStacktrace = `pod: ${podCol}(k8s);${scopedStacktrace}`;
      }

      if (display.namespaceColumn) {
        scopedStacktrace = `namespace: ${n[display.namespaceColumn] || 'UNKNOWN'}(k8s);${scopedStacktrace}`;
      }

      if (display.nodeColumn) {
        scopedStacktrace = `node: ${n[display.nodeColumn] || 'UNKNOWN'}(k8s);${scopedStacktrace}`;
      }

      const splitStack = scopedStacktrace.split(';');
      let currPath = 'all';
      for (const [i, s] of splitStack.entries()) {
        const cleanPath = s.split('(k8s)')[0];
        const path = `${currPath};${cleanPath}`;
        if (!nodeMap[path]) {
          // Set the color based on the language type.
          let lType = 'other';
          if (s.startsWith('[k] ')) {
            lType = 'kernel';
          } else if (s.startsWith('[j] ')) {
            lType = 'java';
          } else if (s.indexOf('(k8s)') !== -1) {
            lType = 'k8s';
          } else if (s.indexOf('.(*') !== -1 || s.indexOf('/') !== -1) {
            lType = 'go';
          } else if (s.indexOf('::') !== -1) {
            lType = 'c';
          } else if (s.indexOf('_[k]') !== -1) {
            lType = 'kernel';
          }

          nodeMap[path] = {
            parent: currPath,
            fullPath: path,
            name: cleanPath,
            count: 0,
            weight: 0,
            percentage: 0,
            color: lType,
          };
        }
        nodeMap[path].percentage += n[display.percentageColumn];

        if (i === splitStack.length - 1) {
          nodeMap[path].weight += n[display.countColumn];
        }
        nodeMap[path].count += n[display.countColumn];
        currPath = path;
      }
      nodeMap.all.count += n[display.countColumn];
    }
    return Object.values(nodeMap);
  };

  return {
    spec,
    hasLegend: false,
    legendColumnName: '',
    preprocess,
    showTooltips: true,
  };
}
