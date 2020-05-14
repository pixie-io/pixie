import { Vis } from 'containers/live/vis';
import * as React from 'react';
import { getQueryParams, setQueryParams } from 'utils/query-params';

import { SetStateFunc } from './common';
import { ScriptContext } from './script-context';
import { VisContext } from './vis-context';

export interface Arguments {
  [arg: string]: string;
}

interface ArgsContextProps {
  args: Arguments;
  setArgs: SetStateFunc<Arguments>;
}

// Populate arguments either from defaultValues or from the input args.
function argsForVis(vis: Vis, args: Arguments, scriptId?: string): Arguments {
  const outArgs: Arguments = {};
  if (!args) {
    args = {};
  }
  for (const variable of vis.variables) {
    const val = typeof args[variable.name] !== 'undefined' ? args[variable.name] : variable.defaultValue;
    outArgs[variable.name] = val;
  }
  if (args.script) {
    outArgs.script = args.script;
  }
  if (scriptId) {
    outArgs.script = scriptId;
  }
  // Compare the two sets of arguments to avoid infinite render cycles.
  if (argsEquals(args, outArgs)) {
    return args;
  }
  return outArgs;
}

function argsEquals(args1: Arguments, args2: Arguments): boolean {
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

export const ArgsContext = React.createContext<ArgsContextProps>(null);

export const ArgsContextProvider = (props) => {
  const { vis } = React.useContext(VisContext);
  const { id } = React.useContext(ScriptContext);

  const [args, setArgsBase] = React.useState<Arguments | null>(() => {
    return argsForVis(vis, getQueryParams(), id);
  });

  const setArgs = (newArgs: Arguments) => {
    setArgsBase(argsForVis(vis, newArgs, id));
  };

  React.useEffect(() => {
    if (args) {
      setQueryParams(args);
    }
  }, [args]);

  React.useEffect(() => {
    setArgsBase(argsForVis(vis, args, id));
  }, [args, vis, id]);

  return (
    <ArgsContext.Provider
      value={{
        args,
        setArgs,
      }}>
      {props.children}
    </ArgsContext.Provider >
  );
};
