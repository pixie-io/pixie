import { VizierQueryError } from 'common/errors';
import { VizierQueryFunc } from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import { useSnackbar } from 'components/snackbar/snackbar';
import { getQueryFuncs, Vis } from 'containers/live/vis';
import * as React from 'react';
import analytics from 'utils/analytics';
import { Arguments } from 'utils/args-utils';
import urlParams from 'utils/url-params';
import { LiveViewPage } from 'containers/live/utils/live-view-params';

import { DataDrawerContext } from './data-drawer-context';
import { ResultsContext } from './results-context';
import { ScriptContext } from './script-context';

interface ExecuteArguments {
  pxl: string;
  vis: Vis;
  args: Arguments;
  id: string;
  title: string;
  liveViewPage?: LiveViewPage;
  entityParamNames?: string[];
  skipURLUpdate?: boolean;
}

interface ExecuteContextProps {
  execute: (execArgs?: ExecuteArguments) => void;
}

export const ExecuteContext = React.createContext<ExecuteContextProps>(null);

export const ExecuteContextProvider = (props) => {
  const { args, vis, pxl, title, id, setScript, commitURL } = React.useContext(ScriptContext);
  const { client, healthy } = React.useContext(ClientContext);
  const { clearResults, setResults, setLoading, loading } = React.useContext(ResultsContext);
  const showSnackbar = useSnackbar();
  const { openDrawerTab } = React.useContext(DataDrawerContext);

  const execute = (execArgs?: ExecuteArguments) => {
    if (loading) {
      showSnackbar({
        message: 'Script is already executing, please wait for it to complete',
        autoHideDuration: 2000,
      });
      return;
    }
    if (!healthy || !client) {
      // TODO(malthus): Maybe link to the admin page to show what is wrong.
      showSnackbar({
        message: 'We are having problems talking to the cluster right now, please try again later',
        autoHideDuration: 5000,
      });
      return;
    }

    setLoading(true);

    if (!execArgs) {
      execArgs = { pxl, vis, args, title, id };
    } else {
      clearResults();

      if (execArgs.liveViewPage != null && execArgs.entityParamNames != null) {
        setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.title,
                  execArgs.liveViewPage, execArgs.entityParamNames);
      } else {
        setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.title);
      }
    }

    let errMsg: string;
    let queryId: string;

    if (!execArgs.skipURLUpdate) {
      commitURL();
    }

    new Promise((resolve, reject) => {
      try {
        resolve(getQueryFuncs(execArgs.vis, execArgs.args));
      } catch (error) {
        reject(error);
      }
    })
      .then((funcs: VizierQueryFunc[]) => {
        return client.executeScript(execArgs.pxl, funcs);
      })
      .then((queryResults) => {
        const newTables = {};
        queryId = queryResults.queryId;
        for (const table of queryResults.tables) {
          newTables[table.name] = table;
        }
        setResults({ tables: newTables, stats: queryResults.executionStats });
      }).catch((error) => {
        if (Array.isArray(error) && error.length) {
          error = error[0];
        }

        const errType = (error as VizierQueryError).errType;
        errMsg = error.message;
        if (errType === 'execution' || !errType) {
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
          query: pxl,
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
