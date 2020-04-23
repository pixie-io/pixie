import {VizierQueryError} from 'common/errors';
import {VizierQueryArg, VizierQueryFunc} from 'common/vizier-grpc-client';

import {ChartDisplay} from './convert-to-vega-spec';
import {ChartPosition, DEFAULT_HEIGHT, GRID_WIDTH} from './layout';

// TODO(nserrino): Replace these with proto when the UI receives protobuf from the script manager
// instead of json from the json bundle.

interface FuncArg {
  name: string;
  value?: string;
  variable?: string;
}

interface Func {
  name: string;
  args: FuncArg[];
}

export const DISPLAY_TYPE_KEY = '@type';
export const TABLE_DISPLAY_TYPE = 'pixielabs.ai/pl.vispb.Table';
export const GRAPH_DISPLAY_TYPE = 'pixielabs.ai/pl.vispb.Graph';

export interface WidgetDisplay {
  readonly '@type': string;
}

interface Widget {
  name?: string;
  position?: ChartPosition;
  func: Func;
  displaySpec: WidgetDisplay;
}

interface Variable {
  name: string;
  type: string;
  defaultValue?: string;
  description?: string;
}

export interface Vis {
  variables: Variable[];
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

function getWidgetArgs(defaults: { [key: string]: string; }, widget: Widget): VizierQueryArg[] {
  const args = [];
  const errors = [];
  widget.func.args.forEach((arg: FuncArg) => {
    if ((arg.value == null) === (arg.variable == null)) {
      errors.push(`Arg '${arg.name}' for function '${widget.func.name}'` +
        `must contain either a value or a reference to a variable`);
      return;
    }

    if (arg.value != null) {
      args.push({
        name: arg.name,
        value: arg.value,
      });
      return;
    }
    // For now, use the default value in the vis.json spec as the value to the function.
    // TODO(nserrino): Support actual variables from the command prompt, or other UI inputs.
    if (!(arg.variable in defaults)) {
      errors.push(`Variable ${arg.variable} does not contain a defaultValue.`);
      return;
    }
    args.push({
      name: arg.name,
      value: defaults[arg.variable],
    });
  });
  if (errors.length > 0) {
    throw new VizierQueryError('vis', errors);
  }
  return args;
}

// This should only be called by vizier grpc client, and it will reject the returned promise
// when executeScript() is called with an invalid Vis spec.
export function getQueryFuncs(vis: Vis, variableValues: { [key: string]: string }): VizierQueryFunc[] {
  const defaults = {};
  vis.variables.forEach((v) => {
    if (v.defaultValue) {
      defaults[v.name] = v.defaultValue;
    }
  });
  const valsOrDefaults = {
    ...defaults,
    ...variableValues,
  };
  return vis.widgets.map((widget, i) => {
    return {
      name: widget.func.name,
      outputTablePrefix: widgetResultName(widget, i),
      args: getWidgetArgs(valsOrDefaults, widget),
    };
  });
}
