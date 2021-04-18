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

import { VizierQueryError } from '@pixie-labs/api';
import { Variable, Vis } from 'containers/live/vis';

export type Arguments = Record<string, string|string[]>;

export function argsEquals(args1: Arguments, args2: Arguments): boolean {
  if (args1 === args2) {
    return true;
  }

  if (args1 === null || args2 === null) {
    return false;
  }

  if (Object.keys(args1).length !== Object.keys(args2).length) {
    return false;
  }
  const args1Map = new Map(Object.entries(args1));
  for (const [key, val] of Object.entries(args2)) {
    if (args1Map.get(key) !== val) {
      return false;
    }
    args1Map.delete(key);
  }
  if (args1Map.size !== 0) {
    return false;
  }
  return true;
}

// Populate arguments either from defaultValues or from the input args.
export function argsForVis(vis: Vis, args: Arguments, scriptId?: string): Arguments {
  if (!vis) {
    return {};
  }
  let inArgs = args;
  const outArgs: Arguments = {};
  if (!args) {
    inArgs = {};
  }
  for (const variable of vis.variables) {
    const val = inArgs[variable.name] != null ? inArgs[variable.name] : variable.defaultValue;
    outArgs[variable.name] = val;
  }
  if (inArgs.script) {
    outArgs.script = inArgs.script;
  }
  if (scriptId) {
    outArgs.script = scriptId;
  }
  return outArgs;
}

export interface ArgTypeMap {
  [arg: string]: string;
}

// Get the types of the given args, according to the provided vis spec.
export function getArgTypesForVis(vis: Vis): ArgTypeMap {
  const types: ArgTypeMap = {};

  vis.variables.forEach((v) => {
    types[v.name] = v.type;
  });
  return types;
}

export interface ArgToVariableMap {
  [arg: string]: Variable;
}

export function getArgVariableMap(vis: Vis): ArgToVariableMap {
  const map: ArgToVariableMap = {};

  vis.variables.forEach((v) => {
    map[v.name] = v;
  });
  return map;
}

export function validateArgValues(vis: Vis, args: Arguments): VizierQueryError {
  if (!vis) {
    return null;
  }
  let inArgs = args;
  if (!args) {
    inArgs = {};
  }
  const errors = [];
  for (const variable of vis.variables) {
    const rawVal: string|string[] = inArgs[variable.name] != null ? inArgs[variable.name] : variable.defaultValue;
    if (variable.validValues && variable.validValues.length) {
      const vals: string[] = Array.isArray(rawVal) ? rawVal : [rawVal];
      for (const val of vals) {
        const validValuesSet = new Set(variable.validValues);
        if (!validValuesSet.has(val)) {
          errors.push(`Value '${val}' passed in for '${variable.name}' is not in `
              + `the set of valid values '${variable.validValues.join(', ')}'.`);
        }
      }
    }
  }

  if (errors.length > 0) {
    return new VizierQueryError('vis', errors);
  }
  return null;
}
