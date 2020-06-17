import { LIVE_VIEW_SCRIPT_ARGS_KEY, useSessionStorage } from 'common/storage';
import * as React from 'react';
import { argsEquals, argsForVis, Arguments } from 'utils/args-utils';
import urlParams from 'utils/url-params';

import { RouteContext } from './route-context';
import { SetStateFunc } from './common';
import { VisContext } from './vis-context';

interface ArgsContextProps {
  args: Arguments;
  setArgs: (newArgs: Arguments, omitList: string[]) => void;
}

export const ArgsContext = React.createContext<ArgsContextProps>(null);

export const ArgsContextProvider = (props) => {
  const { entityParams } = React.useContext(RouteContext);
  const { vis } = React.useContext(VisContext);
  const [args, setArgsBase] = useSessionStorage<Arguments | null>(LIVE_VIEW_SCRIPT_ARGS_KEY, null);

  const setArgs = (newArgs: Arguments, omitList: string[]) => {
    const parsed = argsForVis(vis, newArgs, omitList);
    // Compare the two sets of arguments to avoid infinite render cycles.
    if (!argsEquals(args, parsed)) {
      setArgsBase(parsed);
    }
  };

  React.useEffect(() => {
    if (args) {
      urlParams.setArgs(args);
    }
  }, [args]);

  React.useEffect(() => {
    const parsed = argsForVis(vis, args, Object.keys(entityParams));
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
