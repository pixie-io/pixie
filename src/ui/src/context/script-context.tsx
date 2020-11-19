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
import { useSnackbar } from 'pixie-components';

import {
  getEntityParams, getLiveViewTitle, getNonEntityParams, LiveViewPage,
  LiveViewPageScriptIds, matchLiveViewEntity, toEntityPathname,
} from 'components/live-widgets/utils/live-view-params';
import { BatchDataUpdate, Table } from 'common/vizier-grpc-client';
import { checkExhaustive } from 'utils/check-exhaustive';
import { SetStateFunc } from './common';

import { ResultsContext } from './results-context';
import { LayoutContext } from './layout-context';

// The amount of time we should wait in between mutation retries, in ms.
const mutationRetryMs = 5000; // 5s.

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

const ScriptContextProvider = (props) => {
  const { location } = props;

  const { scripts } = React.useContext(ScriptsContext);
  const { selectedClusterName, setClusterByName, selectedClusterPrettyName } = React.useContext(ClusterContext);
  const { client, healthy } = React.useContext(ClientContext);
  const {
    setResults, setLoading, setStreaming, clearResults, tables: currentResultTables,
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
    // eslint-disable-next-line react-hooks/exhaustive-deps
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

    let errMsg: string;
    let queryId: string;
    let loaded = false;

    const mutation = ContainsMutation(execArgs.pxl);
    const isStreaming = IsStreaming(execArgs.pxl);
    // The cancellation above sets a timeout to mark these both as false, to get around a context update timing issue.
    // In so doing, it pushes that issue downstream, to here, so do the same trick to fix it.
    setTimeout(() => {
      setLoading(true);
      setStreaming(isStreaming);
    });

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

    const onUpdate = (updates: BatchDataUpdate[]) => {
      for (const update of updates) {
        const table: Table = currentResultTables[update.id];
        if (!table) {
          currentResultTables[update.id] = { ...update, data: [update.batch] };
        } else {
          table.data.push(update.batch);
        }
      }
      // Tell React that something changed so that it will re-render
      setResults((results) => ({ ...results }));
    };

    const onResults = (queryResults) => {
      if (queryResults && (isStreaming || queryResults.executionStats)) {
        ({ queryId } = queryResults);
        const newTables = {};
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

        while (numTries > 0) {
          cancelExecution?.();

          let mutationComplete = false;
          let cancelled = false;

          let resolveMutationExecution = null;
          const queryPromise = new Promise((resolve) => { resolveMutationExecution = resolve; });

          const onMutationResults = (queryResults) => {
            if (cancelled) {
              resolveMutationExecution();
              return;
            }

            if (queryResults.mutationInfo?.getStatus().getCode() === GRPCStatusCode.Unavailable) {
              resolveMutationExecution();
              setResults({ tables: {}, mutationInfo: queryResults.mutationInfo });
            } else {
              onResults(queryResults);
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

          client.executeScript(
            execArgs.pxl,
            getQueryFuncs(execArgs.vis, execArgs.args),
            mutation,
          ).subscribe((update) => {
            switch (update.event.type) {
              case 'start':
                setCancelExecution(() => update.cancel);
                break;
              case 'metadata':
              case 'mutation-info':
              case 'stats':
              case 'status':
                onMutationResults(update.results);
                break;
              case 'error':
                onMutationError(update.event.error);
                break;
              case 'data':
              case 'cancel':
                break;
              default:
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                checkExhaustive(update.event!.type);
                break;
            }
          });

          // Wait for the query to get a mutation response before retrying.
          const cancelPromise = new Promise((resolve) => {
            const unwrapped = cancelExecution;
            const cancelFn = () => {
              unwrapped?.();
              setStreaming(false);
              resolve(true);
            };
            setCancelExecution(() => cancelFn);
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
      runMutation().then();
    } else {
      client.executeScript(
        execArgs.pxl,
        getQueryFuncs(execArgs.vis, execArgs.args),
        mutation,
      ).subscribe((update) => {
        switch (update.event.type) {
          case 'start':
            setCancelExecution(() => () => {
              update.cancel();
              setCancelExecution(() => () => {});
              // Timeout so as not to modify one context while rendering another
              setTimeout(() => {
                setStreaming(false);
                setLoading(false);
              });
            });
            break;
          case 'data':
            onUpdate(update.event.data);
            break;
          case 'metadata':
          case 'mutation-info':
          case 'status':
          case 'stats':
            onResults(update.results);
            break;
          case 'error':
            onError(update.event.error);
            break;
          case 'cancel':
            break;
          default:
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            checkExhaustive(update.event!.type);
            break;
        }
      });
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
