import { VizierQueryError } from 'common/errors';
import { VizierQueryFunc } from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import { useSnackbar } from 'components/snackbar/snackbar';
import { getQueryFuncs, Vis } from 'containers/live/vis';
import * as React from 'react';

import { ArgsContext, Arguments } from './args-context';
import { DataDrawerContext } from './data-drawer-context';
import { ResultsContext } from './results-context';
import { ScriptContext } from './script-context';
import { VisContext } from './vis-context';

interface ExecuteArguments {
  script: string;
  vis: Vis;
  args: Arguments;
  id?: string;
  title?: string;
}

interface ExecuteContextProps {
  execute: (execArgs?: ExecuteArguments) => void;
}

export const ExecuteContext = React.createContext<ExecuteContextProps>(null);

export const ExeucteContextProvider = (props) => {
  const { id, script, setIdAndTitle, setScript } = React.useContext(ScriptContext);
  const { vis, setVis } = React.useContext(VisContext);
  const { args, setArgs } = React.useContext(ArgsContext);
  const client = React.useContext(ClientContext);
  const { clearResults, setResults, setLoading, loading } = React.useContext(ResultsContext);
  const showSnackbar = useSnackbar();
  const { openDrawerTab } = React.useContext(DataDrawerContext);

  const execute = (execArgs?: ExecuteArguments) => {
    if (!client || loading) {
      return;
    }

    setLoading(true);

    if (!execArgs) {
      execArgs = { script, vis, args };
    } else {
      setVis(execArgs.vis);
      setScript(execArgs.script);

      if (execArgs.id && execArgs.title) {
        setIdAndTitle(execArgs.id, execArgs.title);
      }

      if (execArgs.args) {
        setArgs(execArgs.args);
      } else {
        // If args were not specified when running the script,
        // use the existing args from the context.
        execArgs.args = args;
      }
    }

    let errMsg: string;
    let queryId: string;

    new Promise((resolve, reject) => {
      try {
        resolve(getQueryFuncs(execArgs.vis, execArgs.args));
      } catch (error) {
        reject(error);
      }
    })
      .then((funcs: VizierQueryFunc[]) => client.executeScript(execArgs.script, funcs))
      .then((queryResults) => {
        const newTables = {};
        queryId = queryResults.queryId;
        for (const table of queryResults.tables) {
          newTables[table.name] = table;
        }
        setResults({ tables: newTables, stats: queryResults.executionStats });
      }).catch((error) => {
        const errType = (error as VizierQueryError).errType;
        errMsg = error.message;
        if (errType === 'execution') {
          showSnackbar({
            message: errMsg,
            action: () => openDrawerTab('errors'),
            actionTitle: 'details',
            autoHideDuration: 5000,
          });
        } else {
          showSnackbar({
            message: errMsg,
            action: () => execute(execArgs),
            actionTitle: 'retry',
            autoHideDuration: 5000,
          });
        }
        setResults({ tables: {}, error });
      }).finally(() => {
        setLoading(false);
        analytics.track('Query Execution', {
          status: errMsg ? 'success' : 'failed',
          query: script,
          queryID: queryId,
          error: errMsg,
          title: id,
        });
      });
  };

  return (
    <ExecuteContext.Provider value={{ execute }}>
      {props.children}
    </ExecuteContext.Provider>
  );
};
