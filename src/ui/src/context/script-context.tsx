import ClusterContext from 'common/cluster-context';
import { Script } from 'utils/script-bundle';
import { ScriptsContext } from 'containers/App/scripts-context';
import {
  LIVE_VIEW_SCRIPT_ARGS_KEY, LIVE_VIEW_SCRIPT_ID_KEY, LIVE_VIEW_PIXIE_SCRIPT_KEY,
  LIVE_VIEW_VIS_SPEC_KEY, useSessionStorage,
} from 'common/storage';
import ClientContext from 'common/vizier-grpc-client-context';
import { VizierQueryError, GRPCStatusCode } from 'common/errors';
import { ContainsMutation, IsStreaming } from 'utils/pxl';

import * as React from 'react';
import { withRouter } from 'react-router';

import {
  parseVis, toJSON, Vis, getQueryFuncs, validateVis,
} from 'containers/live/vis';
import {
  argsEquals, argsForVis, Arguments, validateArgValues,
} from 'utils/args-utils';
import urlParams from 'utils/url-params';
import { useSnackbar } from 'components/snackbar/snackbar';

import { SetStateFunc } from './common';
import {
  EntityURLParams, getLiveViewTitle, LiveViewEntityParams, LiveViewPage,
  LiveViewPageScriptIds, matchLiveViewEntity, toEntityPathname,
} from '../components/live-widgets/utils/live-view-params';

import { ResultsContext } from './results-context';
import { LayoutContext } from './layout-context';

// The amount of time we should wait in between mutation retries, in ms.
const mutationRetryMs = 5000; // 5s.
// The maximum amount of time we should retry the mutation.
const maxMutationRetryMs = 30000; // 30s.

export interface ExecuteArguments {
  pxl: string;
  vis: Vis;
  args: Arguments;
  id: string;
  liveViewPage?: LiveViewPage;
  skipURLUpdate?: boolean;
}

interface ScriptContextProps {
  liveViewPage: LiveViewPage;

  args: Arguments;
  setArgs: SetStateFunc<Arguments>;

  visJSON: string;
  vis: Vis;
  setVis: SetStateFunc<Vis>;

  setCancelExecution: SetStateFunc<() => void>;
  cancelExecution: () => void;

  pxlEditorText: string;
  visEditorText: string;
  setVisEditorText: SetStateFunc<string>;
  setPxlEditorText: SetStateFunc<string>;

  pxl: string;
  setPxl: SetStateFunc<string>;
  title: string;
  id: string;

  setScript: (vis: Vis, pxl: string, args: Arguments, id: string,
    liveViewPage?: LiveViewPage) => void;
  execute: (execArgs: ExecuteArguments) => void;
  saveEditorAndExecute: () => void;
  parseVisOrShowError: (json: string) => Vis | null;
  argsForVisOrShowError: (vis: Vis, args: Arguments, scriptId?: string) => Arguments;
}

export const ScriptContext = React.createContext<ScriptContextProps>(null);

function emptyVis(): Vis {
  return { variables: [], widgets: [], globalFuncs: [] };
}

function getTitleOfScript(scriptId: string, scripts: Map<string, Script>): string {
  if (scripts.has(scriptId)) {
    return scripts.get(scriptId).title;
  }
  return scriptId;
}

function getEntityParams(liveViewPage: LiveViewPage, args: Arguments): EntityURLParams {
  const entityParamNames = LiveViewEntityParams.get(liveViewPage) || new Set();
  const entityParams = {};
  entityParamNames.forEach((paramName: string) => {
    if (args[paramName] != null) {
      entityParams[paramName] = args[paramName];
    }
  });
  return entityParams;
}

function getNonEntityParams(liveViewPage: LiveViewPage, args: Arguments): Arguments {
  const entityParamNames = LiveViewEntityParams.get(liveViewPage) || new Set();
  const nonEntityParams = {};
  Object.keys(args).forEach((argName: string) => {
    if (!entityParamNames.has(argName)) {
      nonEntityParams[argName] = args[argName];
    }
  });
  return nonEntityParams;
}

const ScriptContextProvider = (props) => {
  const { location } = props;

  const { scripts } = React.useContext(ScriptsContext);
  const { selectedClusterName, setClusterByName, selectedClusterPrettyName } = React.useContext(ClusterContext);
  const { client, healthy } = React.useContext(ClientContext);
  const {
    setResults, setLoading, clearResults,
  } = React.useContext(ResultsContext);
  const { editorPanelOpen } = React.useContext(LayoutContext);
  const showSnackbar = useSnackbar();

  const entity = matchLiveViewEntity(location.pathname);
  const [liveViewPage, setLiveViewPage] = React.useState<LiveViewPage>(entity.page);
  const [visEditorText, setVisEditorText] = React.useState<string>(null);
  const [pxlEditorText, setPxlEditorText] = React.useState<string>(null);

  const [cancelExecution, setCancelExecution] = React.useState<() => void>();

  // Args that are not part of an entity.
  const [args, setArgs] = useSessionStorage<Arguments | null>(LIVE_VIEW_SCRIPT_ARGS_KEY, entity.params);

  const [pxl, setPxl] = useSessionStorage(LIVE_VIEW_PIXIE_SCRIPT_KEY, '');
  const [id, setId] = useSessionStorage(LIVE_VIEW_SCRIPT_ID_KEY,
    entity.page === LiveViewPage.Default ? '' : LiveViewPageScriptIds.get(entity.page));

  // We use a separation of visJSON states to prevent infinite update loops from happening.
  // Otherwise, an update in the editor could trigger an update to some other piece of the pie.
  // visJSON should be the incoming state to the vis editor.
  // Vis Raw is the outgoing state from the vis editor.
  const [visJSON, setVisJSONBase] = useSessionStorage<string>(LIVE_VIEW_VIS_SPEC_KEY);
  const [vis, setVisBase] = React.useState(() => {
    if (visJSON) {
      const parsed = parseVis(visJSON);
      if (parsed) {
        return parsed;
      }
    }
    return emptyVis();
  });

  const parseVisOrShowError = (json: string): Vis | null => {
    if (!json || !json.trim().length) {
      return emptyVis();
    }
    const parsed = parseVis(json);
    if (parsed) {
      return parsed;
    }
    setResults({ tables: {}, error: new VizierQueryError('vis', 'Error parsing VisSpec') });
    return null;
  };

  const argsForVisOrShowError = (visToUse: Vis, argsToUse: Arguments, scriptId?: string): Arguments => {
    const argValueErr = validateArgValues(visToUse, argsToUse);
    if (argValueErr) {
      setResults({ tables: {}, error: argValueErr });
      return null;
    }

    return argsForVis(visToUse, argsToUse, scriptId);
  };

  React.useEffect(() => {
    const newArgs = argsForVis(vis, args);
    // Need this check in order to avoid infinite rerender on 'args' changing.
    if (!argsEquals(newArgs, args)) {
      setArgs(newArgs);
    }
  }, [vis, args, setArgs]);

  // title is dependent on whether or not we are in an entity page.
  const title = React.useMemo(() => {
    const entityParams = getEntityParams(liveViewPage, args);
    const newTitle = getLiveViewTitle(getTitleOfScript(id, scripts), liveViewPage,
      entityParams, selectedClusterPrettyName);
    document.querySelector('title').textContent = newTitle;
    return newTitle;
  }, [liveViewPage, scripts, id, args, selectedClusterPrettyName]);

  // Logic to set cluster

  React.useEffect(() => {
    if (entity.clusterName && entity.clusterName !== selectedClusterName) {
      setClusterByName(entity.clusterName);
    }
    // We only want this useEffect to be called the first time the page is loaded.
    // eslint-disable-next-line
  }, []);

  // Logic to set url params when location changes

  React.useEffect(() => {
    if (location.pathname !== '/live') {
      urlParams.triggerOnChange();
    }
  }, [location]);

  // Logic to update entity paths when live view page or cluster changes.

  React.useEffect(() => {
    clearResults();
    const entityParams = getEntityParams(liveViewPage, args);
    const entityURL = {
      clusterName: selectedClusterName,
      page: liveViewPage,
      params: entityParams,
    };
    urlParams.setPathname(toEntityPathname(entityURL));
    // DO NOT ADD clearResults() it destroys the UI.
  }, [selectedClusterName, liveViewPage, args]);

  React.useEffect(() => {
    const scriptID = liveViewPage === LiveViewPage.Default ? id : '';
    urlParams.setScript(scriptID, /* diff */'');
  }, [liveViewPage, id]);

  // Logic to update arguments (which are a combo of entity params and normal args)

  React.useEffect(() => {
    const nonEntityArgs = getNonEntityParams(liveViewPage, args);
    urlParams.setArgs(nonEntityArgs);
  }, [liveViewPage, args]);

  // Logic to update vis spec.

  // Note that this function does not update args, so it should only be called
  // when variables will not be modified (such as for layouts).
  const setVis = (newVis: Vis) => {
    let visToSet = newVis;
    if (!newVis) {
      visToSet = emptyVis();
    }
    setVisJSONBase(toJSON(visToSet));
    setVisBase(visToSet);
  };

  // Logic to update the full script
  const setScript = (newVis: Vis, newPxl: string, newArgs: Arguments, newID: string,
    newLiveViewPage?: LiveViewPage) => {
    setVis(newVis);
    setPxl(newPxl);
    setArgs(newArgs);
    setId(newID);
    if (newLiveViewPage != null) {
      setLiveViewPage(newLiveViewPage);
    }
  };

  // Logic to commit the URL to the history.
  const commitURL = (page: LiveViewPage, urlId: string, urlArgs: Arguments) => {
    // Only show the script as a query arg when we are not on an entity page.
    const scriptId = page === LiveViewPage.Default ? (urlId || '') : '';
    const nonEntityArgs = getNonEntityParams(page, urlArgs);
    urlParams.commitAll(scriptId, '', nonEntityArgs);
  };

  const execute = (execArgs: ExecuteArguments) => {
    if (cancelExecution != null) {
      cancelExecution();
    }

    if (!healthy || !client) {
      // TODO(philkuz): Maybe link to the admin page to show what is wrong.
      showSnackbar({
        message: 'We are having problems talking to the cluster right now, please try again later',
        autoHideDuration: 5000,
      });
      return;
    }

    setLoading(true);

    let errMsg: string;
    let queryId: string;
    let loaded = false;

    const mutation = ContainsMutation(execArgs.pxl);
    const isStreaming = IsStreaming(execArgs.pxl);

    if (!execArgs.skipURLUpdate) {
      commitURL(execArgs.liveViewPage, execArgs.id, execArgs.args);
    }

    try {
      // Make sure vis has proper references.
      if (execArgs.vis) {
        // validateVis errors out on null vis arguments.
        const visErr = validateVis(execArgs.vis, execArgs.args);
        if (visErr) {
          showSnackbar({
            message: 'Invalid Vis spec',
            autoHideDuration: 5000,
          });
          return;
        }
      }
    } catch (error) {
      showSnackbar({
        message: error,
        autoHideDuration: 5000,
      });
      return;
    }

    const onData = (queryResults) => {
      // If the query is not streaming, we should only set the results when the query has completed.
      // This is to prevent unnecessary re-renders of graphs. This should be fixed when we refactor all of
      // our widgets to properly handle streaming queries.
      if (queryResults && (isStreaming || queryResults.executionStats)) {
        const newTables = {};
        ({ queryId } = queryResults);
        queryResults.tables.forEach((table) => {
          newTables[table.name] = table;
        });
        setResults((results) => ({ tables: newTables, stats: queryResults.executionStats, error: results.error }));
        if (!loaded) {
          setLoading(false);
          loaded = true;
        }
      }

      if (queryResults.executionStats) { // For non-streaming queries, the query is complete. Log analytics.
        analytics.track('Query Execution', {
          status: 'success',
          query: execArgs.pxl,
          queryID: queryId,
          title: execArgs.id,
        });
      }
    };

    const onError = (e) => {
      let error = e;
      if (Array.isArray(error) && error.length) {
        error = error[0];
      }

      setResults({ tables: {}, error });
      const { errType } = (error as VizierQueryError);
      errMsg = error.message;

      analytics.track('Query Execution', {
        status: 'failed',
        query: execArgs.pxl,
        queryID: queryId,
        error: errMsg,
        title: execArgs.id,
      });

      if (errType === 'server' || !errType) {
        showSnackbar({
          message: errMsg,
          action: () => execute(execArgs),
          actionTitle: 'retry',
          autoHideDuration: 5000,
        });
      }
      if (!loaded) {
        setLoading(false);
        loaded = true;
      }
    };

    if (mutation) {
      const runMutation = async () => {
        let numTries = 5;
        let queryCancelFn = null;

        while (numTries > 0) {
          if (queryCancelFn != null) {
            queryCancelFn()();
          }

          let mutationComplete = false;
          let cancelled = false;

          let resolveMutationExecution = null;
          const queryPromise = new Promise((resolve) => { resolveMutationExecution = resolve; });

          const onMutationData = (queryResults) => {
            if (cancelled) {
              resolveMutationExecution();
              return;
            }

            if (queryResults.mutationInfo
              && queryResults.mutationInfo.getStatus().getCode() === GRPCStatusCode.Unavailable) {
              resolveMutationExecution();
              setResults({ tables: {}, mutationInfo: queryResults.mutationInfo });
            } else {
              onData(queryResults);
              mutationComplete = true;
            }
          };

          const onMutationError = (error) => {
            resolveMutationExecution();
            if (cancelled) {
              return;
            }

            mutationComplete = true;
            onError(error);
          };

          const newQueryCancelFn = client.executeScript(
            execArgs.pxl,
            getQueryFuncs(execArgs.vis, execArgs.args),
            mutation,
            onMutationData,
            onMutationError,
          );
          queryCancelFn = newQueryCancelFn;
          setCancelExecution(newQueryCancelFn);

          // Wait for the query to get a mutation response before retrying.
          const cancelPromise = new Promise((resolve) => {
            const cancelFn = () => (() => {
              newQueryCancelFn();
              resolve(true);
            });
            setCancelExecution(cancelFn);
          });

          // eslint-disable-next-line
          await Promise.race([queryPromise, cancelPromise]);

          if (mutationComplete) {
            return;
          }

          // eslint-disable-next-line
          const cancel = await Promise.race([
            new Promise((resolve) => setTimeout(resolve, mutationRetryMs)),
            cancelPromise,
          ]);
          if (cancel) {
            cancelled = true;
            break;
          }

          numTries--;
        }
        if (!loaded) {
          setLoading(false);
          loaded = true;
        }

        setResults({ tables: {}, error: new VizierQueryError('execution', 'Deploying tracepoints failed') });
      };
      runMutation();
    } else {
      const cancelFn = client.executeScript(
        execArgs.pxl,
        getQueryFuncs(execArgs.vis, execArgs.args),
        mutation,
        onData,
        onError,
      );
      setCancelExecution(cancelFn);
    }
  };

  // Parses the editor and returns an ExecuteArguments object.
  const getExecArgsFromEditor = (): ExecuteArguments => {
    let execArgs: ExecuteArguments = {
      pxl,
      vis,
      args,
      id,
      liveViewPage,
    };
    if (editorPanelOpen) {
      let parsedVis = vis;
      let pxlVal = pxl;
      // If the editor has been lazy loaded, the editorText of the other thing will be null until it's been opened.
      if (visEditorText !== null) {
        parsedVis = parseVisOrShowError(visEditorText);
        if (!parsedVis) {
          return null;
        }
      }
      if (pxlEditorText !== null) {
        pxlVal = pxlEditorText;
      }

      const parsedArgs = argsForVisOrShowError(parsedVis, args, id);
      if (!parsedArgs) {
        return null;
      }
      execArgs = {
        pxl: pxlVal,
        vis: parsedVis,
        args: parsedArgs,
        id,
        liveViewPage: LiveViewPage.Default,
      };
      // Verify the args are legit.
    } else if (!argsForVisOrShowError(vis, args, id)) {
      return null;
    }
    return execArgs;
  };

  const saveEditorAndExecute = () => {
    const execArgs = getExecArgsFromEditor();
    if (execArgs) {
      setScript(execArgs.vis, execArgs.pxl, execArgs.args, execArgs.id, execArgs.liveViewPage);
      execute(execArgs);
    }
  };

  return (
    <ScriptContext.Provider
      value={{
        liveViewPage,
        args,
        setArgs,
        pxlEditorText,
        visEditorText,
        setPxlEditorText,
        setVisEditorText,
        vis,
        setVis,
        visJSON,
        parseVisOrShowError,
        pxl,
        setPxl,
        title,
        id,
        setScript,
        execute,
        saveEditorAndExecute,
        cancelExecution,
        setCancelExecution,

        argsForVisOrShowError,
      }}
    >
      {props.children}
    </ScriptContext.Provider>
  );
};

export default withRouter(ScriptContextProvider);
