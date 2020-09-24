import { Vis } from 'containers/live/vis';

export interface Arguments {
  [arg: string]: string;
}

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
  const outArgs: Arguments = {};
  if (!vis) {
    return {};
  }
  let inArgs = args;
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
