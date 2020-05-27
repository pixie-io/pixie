import { LIVE_VIEW_SCRIPT_ARGS_KEY, useSessionStorage } from 'common/storage';
import * as React from 'react';
import { argsEquals, argsForVis, Arguments } from 'utils/args-utils';
import { setQueryParams } from 'utils/query-params';

import { SetStateFunc } from './common';
import { ScriptContext } from './script-context';
import { VisContext } from './vis-context';

interface ArgsContextProps {
  args: Arguments;
  setArgs: SetStateFunc<Arguments>;
}

export const ArgsContext = React.createContext<ArgsContextProps>(null);

export const ArgsContextProvider = (props) => {
  const { vis } = React.useContext(VisContext);
  const { id } = React.useContext(ScriptContext);

  const [args, setArgsBase] = useSessionStorage<Arguments | null>(LIVE_VIEW_SCRIPT_ARGS_KEY, null);

  const setArgs = (newArgs: Arguments) => {
    const parsed = argsForVis(vis, newArgs, id);
    // Compare the two sets of arguments to avoid infinite render cycles.
    if (!argsEquals(args, parsed)) {
      setArgsBase(parsed);
    }
  };

  React.useEffect(() => {
    if (args) {
      setQueryParams(args);
    }
  }, [args]);

  React.useEffect(() => {
    const parsed = argsForVis(vis, args, id);
    // Compare the two sets of arguments to avoid infinite render cycles.
    if (!argsEquals(args, parsed)) {
      setArgsBase(parsed);
    }
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
