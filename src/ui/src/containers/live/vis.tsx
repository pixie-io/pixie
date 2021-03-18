import {
  VizierQueryError, VizierQueryArg, VizierQueryFunc, GRPCStatusCode,
} from '@pixie-labs/api';
import { ArgTypeMap, getArgTypesForVis } from 'utils/args-utils';

import { Status } from 'types/generated/vizierapi_pb';
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

export interface Variable {
  name: string;
  type: string;
  defaultValue?: string;
  description?: string;
  validValues?: string[];
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
    // Tslint doesn't allow empty blocks.
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

function getFuncArgs(variableValues: VariableValues, func: Func): VizierQueryArg[] {
  const args: VizierQueryArg[] = [];
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
      value: variableValues[arg.variable].toString(),
    });
  });
  if (errors.length > 0) {
    throw new VizierQueryError('vis', errors);
  }
  return args;
}

type VariableValues = Record<string, string|string[]>;

function preprocessVariables(variableValues: VariableValues, argTypes: ArgTypeMap): VariableValues {
  const processedVariables: VariableValues = {};

  for (const [argName, argVal] of Object.entries(variableValues)) {
    // Special parsing for string lists.
    if (argTypes[argName] === 'PX_STRING_LIST') {
      const elms = argVal.toString().split(',');
      const listJoined = elms.map((elm) => `'${elm}'`).join(',');
      const listRepr = `[${listJoined}]\n`;
      processedVariables[argName] = listRepr;
      break;
    } else {
      processedVariables[argName] = argVal;
    }
  }
  return processedVariables;
}

// This should only be called by table grpc client, and it will reject the returned promise
// when executeScript() is called with an invalid Vis spec or if that spec is violated.
export function getQueryFuncs(vis: Vis, variableValues: VariableValues): VizierQueryFunc[] {
  const defaults = {};
  if (!vis) {
    return [];
  }
  const missingRequiredArgs: string[] = [];

  vis.variables.forEach((v) => {
    if (typeof v.defaultValue === 'string') {
      defaults[v.name] = v.defaultValue;
    } else if (typeof v.defaultValue === 'undefined' && !String(variableValues[v.name] ?? '').trim()) {
      missingRequiredArgs.push(v.name);
    }
  });

  if (missingRequiredArgs.length) {
    const message = `Specify missing argument(s): ${missingRequiredArgs.join(', ')}`;
    const status = new Status().setCode(GRPCStatusCode.InvalidArgument);
    status.setMessage(message);
    const err = new VizierQueryError('execution', '', status);
    err.message = message;
    throw err;
  }

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
export function validateVis(vis: Vis, variableValues: VariableValues): VizierQueryError[] {
  if (!vis) {
    return [new VizierQueryError('vis', 'null vis object unhandled')];
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
        errors.push(new VizierQueryError('vis',
          `"${widget.name}" may only have one of "func" and "globalFuncOutputName"`));
      }
      if (!globalFuncNames.has(widget.globalFuncOutputName)) {
        errors.push(new VizierQueryError('vis',
          `globalFunc "${widget.globalFuncOutputName}" referenced by "${widget.name}" not found`));
      }
    }
  });

  // TODO(philkuz) wondering if I should keep this or remove it because we typically call this afterwards.
  // Alternatively, we could have getQueryFuncs call the above.
  try {
    getQueryFuncs(vis, variableValues);
  } catch (error) {
    errors.push(error as VizierQueryError);
  }

  return errors;
}
