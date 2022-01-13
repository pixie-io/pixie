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

import { VizierQueryError } from 'app/api';
import { Variable, Vis } from 'app/containers/live/vis';

export type Arguments = Record<string, string | string[]>;

/** Deep equality test for two sets of script arguments */
export function argsEquals(args1: Arguments, args2: Arguments): boolean {
  if (args1 === args2) return true;
  if (args1 === null || args2 === null) return false;
  if (Object.keys(args1).length !== Object.keys(args2).length) return false;

  const args1Map = new Map(Object.entries(args1));
  for (const [key, val] of Object.entries(args2)) {
    if (args1Map.get(key) !== val) {
      return false;
    }
    args1Map.delete(key);
  }

  return args1Map.size === 0;
}

/**
 * Checks that all variables in the vis spec are specified with valid values (or have a default), and that no extra
 * args were provided that don't exist in the vis spec.
 */
export function validateArgs(vis: Vis, args: Arguments): VizierQueryError | null {
  if (!vis?.variables.length) {
    return null;
  }

  const errors: string[] = [];

  for (const { name, defaultValue, validValues } of (vis?.variables ?? [])) {
    if (defaultValue == null && (args[name] == null || args[name] === '')) {
      errors.push(`Missing value for required arg \`${name}\`.`);
    } else if (validValues?.length) {
      let vals = defaultValue ?? args[name];
      if (!Array.isArray(vals)) vals = [vals];
      for (const val of vals) {
        if (!validValues.includes(val)) {
          errors.push(`Arg ${name} does not permit value ${val}. Allowed values: ${validValues.join(', ')}`);
        }
      }
    }
  }

  const knownVars = vis.variables.map((v) => v.name);
  for (const argName of Object.keys(args)) {
    if (!knownVars.includes(argName) && args[argName] !== undefined) {
      errors.push(`Unrecognized arg ${argName} was provided.`);
    }
  }

  if (errors.length > 0) {
    return new VizierQueryError('vis', errors);
  }
  return null;
}

/**
 * Merges the given arguments with the defaults for the given Vis spec. Strips/ignores extraneous args.
 * This does NOT check validateArgPresenceAndValues; it's recommended to check that first if trying to run a script.
 */
export function argsForVis(vis: Vis, args: Arguments): Arguments {
  if (!vis?.variables.length) {
    return {};
  }

  const outArgs: Arguments = {};
  for (const { name, defaultValue } of vis.variables) {
    // Note: if defaultValue is undefined, the arg is required.
    outArgs[name] = (args[name] || defaultValue) ?? '';
  }

  return outArgs;
}

export interface ArgTypeMap {
  [arg: string]: string;
}

/** Extracts the type of each variable in the given vis spec. */
export function argTypesForVis(vis: Vis): ArgTypeMap {
  if (!vis?.variables.length) {
    return {};
  }

  return vis.variables.reduce((a, c) => ({ ...a, [c.name]: c.type }), {});
}

export interface ArgToVariableMap {
  [arg: string]: Variable;
}

/** Creates a record of variable name -> variable definition from the given vis spec. */
export function argVariableMap(vis: Vis): ArgToVariableMap {
  if (!vis?.variables.length) {
    return {};
  }

  return vis.variables.reduce((a, c) => ({ ...a, [c.name]: c }), {});
}

/**
 * Serializes arguments such that order of keys is retained. Useful to prevent re-renders when args didn't change value.
 * Don't wrap this in a React.useMemo(() => ..., [args]). Doing so causes a re-render, defeating the purpose.
 */
export function stableSerializeArgs(args: Arguments): string {
  return JSON.stringify(args, Object.keys(args ?? {}).sort());
}
