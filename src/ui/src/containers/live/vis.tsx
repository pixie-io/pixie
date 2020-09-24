import { VizierQueryError } from 'common/errors';
import { VizierQueryArg, VizierQueryFunc } from 'common/vizier-grpc-client';
import { ArgTypeMap, getArgTypesForVis } from 'utils/args-utils';

import { ChartPosition } from './layout';

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
export const REQUEST_GRAPH_DISPLAY_TYPE = 'pixielabs.ai/pl.vispb.RequestGraph';

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
      // TODO(nserrino): Do actual validation that this object matches the vis.proto json schema.
      if (!parsed.variables) {
        parsed.variables = [];
      }
      if (!parsed.widgets) {
        parsed.widgets = [];
      }
      if (!parsed.globalFuncs) {
        parsed.globalFuncs = [];
      }
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

function getFuncArgs(variableValues: { [key: string]: string }, func: Func): VizierQueryArg[] {
  const args = [];
  const errors = [];
  func.args.forEach((arg: FuncArg) => {
    if ((arg.value == null) === (arg.variable == null)) {
      errors.push(`Arg "${arg.name}" of "${func.name}()" `
        + 'needs either a value or a variable reference');
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
    if (!(arg.variable in variableValues)) {
      errors.push(`Arg "${arg.name}" of "${func.name}()" references undefined variable "${arg.variable}"`);
      return;
    }
    args.push({
      name: arg.name,
      value: variableValues[arg.variable],
    });
  });
  if (errors.length > 0) {
    throw new VizierQueryError('vis', errors);
  }
  return args;
}

interface VariableValues {
  [key: string]: string;
}

function preprocessVariables(variableValues: VariableValues, argTypes: ArgTypeMap): VariableValues {
  const processedVariables: VariableValues = {};

  Object.entries(variableValues).forEach(([argName, argVal]) => {
    // Special parsing for string lists.
    if (argTypes[argName] === 'PX_STRING_LIST') {
      const elms = argVal.split(',');
      const listJoined = elms.map((elm) => `'${elm}'`).join(',');
      const listRepr = `[${listJoined}]\n`;
      processedVariables[argName] = listRepr;
      return;
    }
    processedVariables[argName] = argVal;
  });
  return processedVariables;
}

// This should only be called by table grpc client, and it will reject the returned promise
// when executeScript() is called with an invalid Vis spec.
export function getQueryFuncs(vis: Vis, variableValues: VariableValues): VizierQueryFunc[] {
  const defaults = {};
  if (!vis) {
    return [];
  }
  vis.variables.forEach((v) => {
    if (typeof v.defaultValue === 'string') {
      defaults[v.name] = v.defaultValue;
    }
  });
  const unprocessedValsOrDefaults = {
    ...defaults,
    ...variableValues,
  };
  const argTypes = getArgTypesForVis(vis);
  const valsOrDefaults = preprocessVariables(unprocessedValsOrDefaults, argTypes);

  let visGlobalFuncs = vis.globalFuncs;
  if (!vis.globalFuncs) {
    visGlobalFuncs = [];
  }

  const globalFuncs = visGlobalFuncs.map((globalFunc) => ({
    name: globalFunc.func.name,
    // There shouldn't be any confusion over this name, outputName is a required field
    // and should be validated before reaching this point.
    outputTablePrefix: globalFunc.outputName,
    args: getFuncArgs(valsOrDefaults, globalFunc.func),
  }));
  // We filter out widgets that don't have function definitions.
  const widgetFuncs = vis.widgets.filter((widget) => widget.func).map((widget, i) => ({
    name: widget.func.name,
    outputTablePrefix: widgetTableName(widget, i),
    args: getFuncArgs(valsOrDefaults, widget.func),
  }));
  return globalFuncs.concat(widgetFuncs);
}

export function toJSON(vis: Vis) {
  return JSON.stringify(vis, null, 2);
}

// Validate Vis makes sure vis is correctly specified or throws an error.
export function validateVis(vis: Vis, variableValues: { [key: string]: string }): VizierQueryError {
  if (!vis) {
    return new VizierQueryError('vis', 'null vis object unhandled');
  }
  const globalFuncNames = new Set();
  vis.globalFuncs.forEach((globalFunc) => {
    globalFuncNames.add(globalFunc.outputName);
  });

  // Verify that functions have only one of (globalFuncOutputName, func)
  // and that globalFuncOutputNames are valid.
  const errors = [];
  vis.widgets.forEach((widget) => {
    if (widget.globalFuncOutputName) {
      if (widget.func) {
        errors.push(`"${widget.name}" may only have one of "func" and "globalFuncOutputName"`);
      }
      if (!globalFuncNames.has(widget.globalFuncOutputName)) {
        errors.push(`globalFunc "${widget.globalFuncOutputName}" referenced by "${widget.name}" not found`);
      }
    }
  });

  // TODO(philkuz) wondering if I should keep this or remove it because we typically call this afterwards.
  // Alternatively, we could have getQueryFuncs call the above.
  try {
    getQueryFuncs(vis, variableValues);
  } catch (error) {
    const { details } = error as VizierQueryError;
    if (Array.isArray(details)) {
      errors.push(...details);
    } else if (typeof details === 'string') {
      errors.push(details);
    }
  }

  if (errors.length > 0) {
    return new VizierQueryError('vis', errors);
  }

  return null;
}
