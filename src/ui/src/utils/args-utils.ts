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
// omitList is used to filter out args that are passed in via entityParams.
export function argsForVis(vis: Vis, args: Arguments, omitList: string[], scriptId?: string): Arguments {
  const outArgs: Arguments = {};
  if (!vis) {
    return {};
  }
  if (!args) {
    args = {};
  }
  for (const variable of vis.variables) {
    if (omitList.indexOf(variable.name) >= 0) {
      continue;
    }
    const val = typeof args[variable.name] !== 'undefined' ? args[variable.name] : variable.defaultValue;
    outArgs[variable.name] = val;
  }
  if (args.script) {
    outArgs.script = args.script;
  }
  if (scriptId) {
    outArgs.script = scriptId;
  }
  return outArgs;
}
