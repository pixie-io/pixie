import { LIVE_VIEW_SCRIPT_ARGS_KEY, useSessionStorage } from 'common/storage';
import * as React from 'react';
import { argsEquals, argsForVis, Arguments } from 'utils/args-utils';
import QueryParams from 'utils/query-params';

import { SetStateFunc } from './common';
import { VisContext } from './vis-context';

interface ArgsContextProps {
  args: Arguments;
  setArgs: SetStateFunc<Arguments>;
}

export const ArgsContext = React.createContext<ArgsContextProps>(null);

export const ArgsContextProvider = (props) => {
  const { vis } = React.useContext(VisContext);

  const [args, setArgsBase] = useSessionStorage<Arguments | null>(LIVE_VIEW_SCRIPT_ARGS_KEY, null);

  const setArgs = (newArgs: Arguments) => {
    const parsed = argsForVis(vis, newArgs);
    // Compare the two sets of arguments to avoid infinite render cycles.
    if (!argsEquals(args, parsed)) {
      setArgsBase(parsed);
    }
  };

  React.useEffect(() => {
    if (args) {
      QueryParams.setArgs(args);
    }
  }, [args]);

  React.useEffect(() => {
    const parsed = argsForVis(vis, args);
    // Compare the two sets of arguments to avoid infinite render cycles.
    if (!argsEquals(args, parsed)) {
      setArgsBase(parsed);
    }
  }, [args, vis]);

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
