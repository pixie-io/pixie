import { VizierQueryError } from 'common/errors';
import * as ls from 'common/localstorage';
import { Table, VizierQueryFunc } from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import { SnackbarProvider, useSnackbar } from 'components/snackbar/snackbar';
import * as React from 'react';
import { QueryExecutionStats } from 'types/generated/vizier_pb';
import { getQueryParams, setQueryParams } from 'utils/query-params';

import { DataDrawerContext, DataDrawerContextProvider } from './context/data-drawer-context';
import { LayoutContextProvider } from './context/layout-context';
import { getQueryFuncs, parseVis, Vis } from './vis';

// Type alias to clean up domain management.
type Domain = [number, number] | null;
type DomainFn = ((domain: Domain) => Domain);
interface LiveContextProps {
  vizierReady: boolean;
  setScripts: (script: string, vis: string, title: Title, args: Arguments) => void;
  executeScript: (script?: string, vis?: Vis, args?: Arguments) => void;
  updateScript: (code: string) => void;
  updateVis: (spec: Vis) => void;
  setHoverTime: (time: number) => void;
  setTSDomain: (domain: Domain | DomainFn) => void;
}

interface Tables {
  [name: string]: Table;
}

interface Results {
  error?: Error;
  tables: Tables;
  stats?: QueryExecutionStats;
}

interface Title {
  title: string;
  id: string;
}

interface Arguments {
  [arg: string]: string;
}

interface ArgsContextProps {
  args: Arguments;
  setArgs: (args: Arguments) => void;
}

export const ScriptContext = React.createContext<string>('');
export const ResultsContext = React.createContext<Results>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);
export const TitleContext = React.createContext<Title>(null);
export const VisContext = React.createContext<Vis>(null);
export const ArgsContext = React.createContext<ArgsContextProps>(null);
export const HoverTimeContext = React.createContext<number>(null);
export const TSDomainContext = React.createContext<number[]>([]);

// TODO(malthus): Move these into a separate module.
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

const LiveContextProvider = (props) => {
  const [script, setScript] = ls.useLocalStorage(ls.LIVE_VIEW_PIXIE_SCRIPT_KEY, '');
  const [tsDomain, setTSDomain] = React.useState<Domain>(null);

  const [results, setResults] = React.useState<Results>({ tables: {} });

  const [vis, setVis] = React.useState<Vis>(parseVis(ls.getLiveViewVisSpec()) || { variables: [], widgets: [] });
  React.useEffect(() => {
    ls.setLiveViewVisSpec(JSON.stringify(vis, null, 2));
  }, [vis]);

  const setScripts = React.useCallback((newScript, newVis, newTitle, newArgs) => {
    setScript(newScript);
    setTitle(newTitle);
    const parsedVis = parseVis(newVis) || { variables: [], widgets: [] };
    setVis(parsedVis);
    setArgsRaw(argsForVis(parsedVis, newArgs, newTitle.id));
  }, []);

  const [title, setTitle] = ls.useLocalStorage<Title>(ls.LIVE_VIEW_TITLE_KEY, null);

  // setArgsRaw sets the args without considering the context of the vis.
  // the exported setArgs listens to the vis and only sets those args which are specified in the vis.
  const [args, setArgsRaw] = React.useState<Arguments | null>(() => {
    return getQueryParams();
  });

  React.useEffect(() => {
    // TODO(malthus): Remove this once we switch to eslint.
    // tslint:disable-next-line:whitespace
    setArgsRaw(argsForVis(vis, args, title?.id));
  }, [vis, args, title]);

  const argsContext = React.useMemo(() => {
    return {
      // Only return the arguments that apply to the current vis spec.
      args: argsForVis(vis, args),
      setArgs: (inputArgs: Arguments) => {
        return setArgsRaw(argsForVis(vis, inputArgs));
      },
    };
  }, [args, setArgsRaw, vis]);

  React.useEffect(() => {
    if (args) {
      setQueryParams(args);
    }
  }, [args]);

  const client = React.useContext(ClientContext);

  const showSnackbar = useSnackbar();

  const { openDrawerTab } = React.useContext(DataDrawerContext);

  const executeScript = React.useCallback((inputScript?: string, inputVis?: Vis, inputArgs?: Arguments) => {
    if (!client) {
      return;
    }

    let errMsg: string;
    let queryId: string;

    setResults({tables: {}});

    new Promise((resolve, reject) => {
      try {
        resolve(getQueryFuncs(typeof inputVis === 'undefined' ? vis : inputVis, inputArgs || args || {}));
      } catch (error) {
        reject(error);
      }
    })
      .then((funcs: VizierQueryFunc[]) => client.executeScript(inputScript || script, funcs))
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
            action: () => executeScript(inputScript),
            actionTitle: 'retry',
            autoHideDuration: 5000,
          });
        }
        setResults({ tables: {}, error });
      }).finally(() => {
        analytics.track('Query Execution', {
          status: errMsg ? 'success' : 'failed',
          query: script,
          queryID: queryId,
          error: errMsg,
          title,
        });
      });
  }, [client, script, vis, title, args]);

  const [hoverTime, setHoverTime] = React.useState<number>(null);

  const liveViewContext = React.useMemo(() => ({
    updateScript: setScript,
    vizierReady: !!client,
    setScripts,
    executeScript,
    updateVis: setVis,
    setHoverTime,
    setTSDomain,
  }), [executeScript, client]);

  return (
    <LiveContext.Provider value={liveViewContext}>
      <TitleContext.Provider value={title}>
        <ArgsContext.Provider value={argsContext}>
          <ScriptContext.Provider value={script}>
            <ResultsContext.Provider value={results}>
              <VisContext.Provider value={vis}>
                <HoverTimeContext.Provider value={hoverTime}>
                  <TSDomainContext.Provider value={tsDomain}>
                    {props.children}
                  </TSDomainContext.Provider>
                </HoverTimeContext.Provider>
              </VisContext.Provider>
            </ResultsContext.Provider>
          </ScriptContext.Provider>
        </ArgsContext.Provider>
      </TitleContext.Provider>
    </LiveContext.Provider>
  );
};

export function withLiveContextProvider(WrappedComponent) {
  return () => (
    <SnackbarProvider>
      <LayoutContextProvider>
        <DataDrawerContextProvider>
          <LiveContextProvider>
            <WrappedComponent />
          </LiveContextProvider>
        </DataDrawerContextProvider>
      </LayoutContextProvider>
    </SnackbarProvider>
  );
}
