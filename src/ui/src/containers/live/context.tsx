import * as ls from 'common/localstorage';
import {Table, VizierQueryFunc} from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import {SnackbarProvider, useSnackbar} from 'components/snackbar/snackbar';
import {parseSpecs, VisualizationSpecMap} from 'components/vega/spec';
import * as React from 'react';

import {parsePlacementOld, Placement} from './layout';
import {getQueryFuncs, parseVis, Vis} from './vis';

interface LiveContextProps {
  // Old live view format functions.
  updateVegaSpecOld: (spec: VisualizationSpecMap) => void;
  updatePlacementOld: (placement: Placement) => void;
  executeScriptOld: (script?: string) => void;
  setScriptsOld: (script: string, vega: string, placement: string, title: Title) => void;

  // Shared between old and new live view functions.
  updateScript: (code: string) => void;
  vizierReady: boolean;

  // New live view functions.
  executeScript: (script?: string, vis?: Vis) => void;
  setScripts: (script: string, vis: string, title: Title) => void;
  updateVis: (spec: Vis) => void;

  // Temporary bool that allows the UI to know whether the current live view is old or new mode.
  oldLiveViewMode: boolean;
}

interface Tables {
  [name: string]: Table;
}

interface Results {
  error?: Error;
  tables: Tables;
}

interface Title {
  title: string;
  id: string;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContextOld = React.createContext<VisualizationSpecMap>(null);
export const PlacementContextOld = React.createContext<Placement>(null);
export const ResultsContext = React.createContext<Results>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);
export const TitleContext = React.createContext<Title>(null);
export const VisContext = React.createContext<Vis>(null);

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>(ls.getLiveViewPixieScript());
  React.useEffect(() => {
    ls.setLiveViewPixieScript(script);
  }, [script]);

  const [vegaSpec, setVegaSpecOld] = React.useState<VisualizationSpecMap>(
    parseSpecs(ls.getLiveViewVegaSpecOld()) || {});

  const [placement, setPlacementOld] = React.useState<Placement>(
    parsePlacementOld(ls.getLiveViewPlacementSpecOld()) || {});

  const [results, setResults] = React.useState<Results>({ tables: {} });

  const [vis, setVis] = React.useState<Vis>(parseVis(ls.getLiveViewVisSpec()) || { variables: [], widgets: [] });
  React.useEffect(() => {
    ls.setLiveViewVisSpec(JSON.stringify(vis, null, 2));
  }, [vis]);

  const [oldLiveViewMode, setOldLiveViewMode] = React.useState<boolean>(ls.getOldLiveViewMode());
  React.useEffect(() => {
    ls.setOldLiveViewMode(oldLiveViewMode);
  }, [oldLiveViewMode]);

  const setScriptsOld = React.useCallback((newScript, newVega, newPlacement, newTitle) => {
    setOldLiveViewMode(true);
    setScript(newScript);
    setTitle(newTitle);
    setVegaSpecOld(parseSpecs(newVega) || {});
    setPlacementOld(parsePlacementOld(newPlacement) || {});
  }, []);

  const setScripts = React.useCallback((newScript, newVis, newTitle) => {
    setOldLiveViewMode(false);
    setScript(newScript);
    setTitle(newTitle);
    setVis(parseVis(newVis) || { variables: [], widgets: [] });
  }, []);

  const [title, setTitle] = React.useState<Title>(ls.getLiveViewTitle());
  React.useEffect(() => {
    ls.setLiveViewTitle(title);
  }, [title]);

  const client = React.useContext(ClientContext);

  const showSnackbar = useSnackbar();

  const executeScriptOld = React.useCallback((inputScript?: string) => {
    if (!client) {
      return;
    }

    let errMsg: string;
    let queryId: string;

    client.executeScriptOld(inputScript || script).then((queryResults) => {
      const newTables = {};
      queryId = queryResults.queryId;
      for (const table of queryResults.tables) {
        newTables[table.name] = table;
      }
      setResults({ tables: newTables });
    }).catch((error) => {
      errMsg = error.message;
      setResults({ tables: {}, error });
      showSnackbar({
        message: errMsg,
        action: () => executeScriptOld(inputScript),
        actionTitle: 'retry',
        autoHideDuration: 5000,
      });
    }).finally(() => {
      analytics.track('Query Execution', {
        status: errMsg ? 'success' : 'failed',
        query: script,
        queryID: queryId,
        error: errMsg,
        title,
      });
    });
  }, [client, script, title]);

  const executeScript = React.useCallback((inputScript?: string, inputVis?: Vis) => {
    if (!client) {
      return;
    }

    let errMsg: string;
    let queryId: string;

    new Promise((resolve, reject) => {
      try {
        resolve(getQueryFuncs(inputVis || vis));
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
        setResults({ tables: newTables });
      }).catch((error) => {
        errMsg = error.message;
        setResults({ tables: {}, error });
        showSnackbar({
          message: errMsg,
          // TODO(malthus): It doesn't make sense to always show retry.
          // Make the action to show the error.
          action: () => executeScript(inputScript),
          actionTitle: 'retry',
          autoHideDuration: 5000,
        });
      }).finally(() => {
        analytics.track('Query Execution', {
          status: errMsg ? 'success' : 'failed',
          query: script,
          queryID: queryId,
          error: errMsg,
          title,
        });
      });
  }, [client, script, vis, title]);

  const liveViewContext = React.useMemo(() => ({
    // Old Live View format
    updateVegaSpecOld: setVegaSpecOld,
    updatePlacementOld: setPlacementOld,
    setScriptsOld,

    // Shared between old and new Live View format
    updateScript: setScript,
    vizierReady: !!client,

    // New Live view format
    setScripts,
    executeScriptOld,
    executeScript,
    updateVis: setVis,

    // temporary
    oldLiveViewMode,
  }), [executeScript, client]);

  return (
    <LiveContext.Provider value={liveViewContext}>
      <TitleContext.Provider value={title}>
        <ScriptContext.Provider value={script}>
          <VegaContextOld.Provider value={vegaSpec}>
            <PlacementContextOld.Provider value={placement}>
              <ResultsContext.Provider value={results}>
                <VisContext.Provider value={vis}>
                  {props.children}
                </VisContext.Provider>
              </ResultsContext.Provider>
            </PlacementContextOld.Provider>
          </VegaContextOld.Provider>
        </ScriptContext.Provider>
      </TitleContext.Provider>
    </LiveContext.Provider>
  );
};

export function withLiveContextProvider(WrappedComponent) {
  return () => (
    <SnackbarProvider>
      <LiveContextProvider>
        <WrappedComponent />
      </LiveContextProvider>
    </SnackbarProvider>
  );
}
