import * as React from 'react';
import { argsForVis, Arguments } from 'utils/args-utils';
import { getQueryParams, setQueryParams } from 'utils/query-params';

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
