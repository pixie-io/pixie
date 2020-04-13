import {VizierQueryFunc} from 'common/vizier-grpc-client';

import {ChartDisplay} from './convert-to-vega-spec';
import {ChartPosition, DEFAULT_HEIGHT, GRID_WIDTH} from './layout';

// TODO(nserrino): Replace these with proto when the UI receives protobuf from the script manager
// instead of json from the json bundle.

interface FuncArg {
  name: string;
  value: string;
}

interface Func {
  name: string;
  args: FuncArg[];
}

export const DISPLAY_TYPE_KEY = '@type';
export const TABLE_DISPLAY_TYPE = 'pixielabs.ai/pl.vispb.Table';

export interface WidgetDisplay {
  readonly '@type': string;
}

interface Widget {
  name?: string;
  position?: ChartPosition;
  func: Func;
  displaySpec: WidgetDisplay;
}

export interface Vis {
  widgets: Widget[];
}

export function parseVis(json: string): Vis {
  try {
    const parsed = JSON.parse(json);
    if (typeof parsed === 'object') {
      return parsed as Vis;
    }
  } catch (e) {
    // noop. tslint doesn't allow empty blocks.
  }
  return null;
}

export function widgetResultName(widget: Widget, widgetIndex: number): string {
  if (widget.name) {
    return widget.name;
  }
  return `widget_${widgetIndex}`;
}

export function getQueryFuncs(vis: Vis): VizierQueryFunc[] {
  return vis.widgets.map((widget, i) => {
    return {
      name: widget.func.name,
      outputTablePrefix: widgetResultName(widget, i),
      args: widget.func.args,
    };
  });
}
