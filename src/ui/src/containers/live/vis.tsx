import { VizierQueryError } from 'common/errors';
import { VizierQueryArg, VizierQueryFunc } from 'common/vizier-grpc-client';

import { ChartDisplay } from './convert-to-vega-spec';
import { ChartPosition, DEFAULT_HEIGHT, GRID_WIDTH } from './layout';

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

export interface Widget {
  name?: string;
  position?: ChartPosition;
  func?: Func;
  globalFuncOutputName?: string;
  displaySpec: WidgetDisplay;
}

interface Variable {
  name: string;
  type: string;
  defaultValue?: string;
  description?: string;
}

interface GlobalFunc {
  outputName: string;
  func: Func;
}

export interface Vis {
  variables: Variable[];
  widgets: Widget[];
  globalFuncs: GlobalFunc[];
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

// Gets the name of the table backing this widget. It will either be globalFuncOutputName, the name
// of the widget, or "widget_{index}"".
export function widgetTableName(widget: Widget, widgetIndex: number): string {
  if (widget.globalFuncOutputName) {
    return widget.globalFuncOutputName;
  }
  if (widget.name) {
    return widget.name;
  }
  return `widget_${widgetIndex}`;
}

function getFuncArgs(defaults: { [key: string]: string; }, func: Func): VizierQueryArg[] {
  const args = [];
  const errors = [];
  func.args.forEach((arg: FuncArg) => {
    if ((arg.value == null) === (arg.variable == null)) {
      errors.push(`Arg '${arg.name}' for function '${func.name}'` +
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
  if (!vis) {
    return [];
  }
  vis.variables.forEach((v) => {
    if (typeof v.defaultValue === 'string') {
      defaults[v.name] = v.defaultValue;
    }
  });
  const valsOrDefaults = {
    ...defaults,
    ...variableValues,
  };

  if (!vis.globalFuncs) {
    vis.globalFuncs = [];
  }

  const globalFuncs = vis.globalFuncs.map((globalFunc, i) => {
    return {
      name: globalFunc.func.name,
      // There shouldn't be any confusion over this name, outputName is a required field
      // and should be validated before reaching this point.
      outputTablePrefix: globalFunc.outputName,
      args: getFuncArgs(valsOrDefaults, globalFunc.func),
    };
  });
  // We filter out widgets that don't have function definitions.
  const widgetFuncs =  vis.widgets.filter((widget) => {
    return widget.func;
  }).map((widget, i) => {
    return {
      name: widget.func.name,
      outputTablePrefix: widgetTableName(widget, i),
      args: getFuncArgs(valsOrDefaults, widget.func),
    };
  });
  return globalFuncs.concat(widgetFuncs);
}
