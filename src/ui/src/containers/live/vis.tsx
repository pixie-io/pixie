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

import {
  VizierQueryError, VizierQueryArg, VizierQueryFunc, GRPCStatusCode,
} from 'app/api';
import { ArgTypeMap, argTypesForVis } from 'app/utils/args-utils';

import { Status } from 'app/types/generated/vizierapi_pb';
import { ChartPosition } from './layout';

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
export const TABLE_DISPLAY_TYPE = 'types.px.dev/px.vispb.Table';
export const GRAPH_DISPLAY_TYPE = 'types.px.dev/px.vispb.Graph';
export const REQUEST_GRAPH_DISPLAY_TYPE = 'types.px.dev/px.vispb.RequestGraph';

export interface WidgetDisplay {
  readonly '@type': string;
}

export interface Widget {
  name: string;
  position?: ChartPosition;
  func?: Func;
  globalFuncOutputName?: string;
  displaySpec: WidgetDisplay;
}

interface StringValue {
  value: string;
}

export interface Variable {
  name: string;
  type: string;
  defaultValue?: StringValue;
  description: string;
  validValues: string[];
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

// Parses vis and errors out instead of silently hiding the error.
export function parseVis(json: string): Vis {
  if (!json) {
    return {
      variables: [],
      widgets: [],
      globalFuncs: [],
    };
  }
  const parsed = JSON.parse(json);
  if (typeof parsed !== 'object') {
    throw new VizierQueryError('vis', 'did not parse object');
  }
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
      // noinspection UnnecessaryLocalVariableJS
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
    if (v.defaultValue != null) {
      defaults[v.name] = v.defaultValue.value;
    } else if (!String(variableValues[v.name] ?? '').trim()) {
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
  const argTypes = argTypesForVis(vis);
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

export function toJSON(vis: Vis): string {
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
