import * as ls from 'common/localstorage';
import {Table} from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import {SnackbarProvider, useSnackbar} from 'components/snackbar/snackbar';
import {parseSpecs, VisualizationSpecMap} from 'components/vega/spec';
import * as React from 'react';

import {parsePlacement, Placement} from './layout';

interface LiveContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (spec: VisualizationSpecMap) => void;
  updatePlacement: (placement: Placement) => void;
  vizierReady: boolean;
  executeScript: (script?: string) => void;
  setScripts: (script: string, vega: string, placement: string, title: Title) => void;
}

interface Tables {
  [name: string]: Table;
}

interface Title {
  title: string;
  id: string;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<VisualizationSpecMap>(null);
export const PlacementContext = React.createContext<Placement>(null);
export const ResultsContext = React.createContext<Tables>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);
export const TitleContext = React.createContext<Title>(null);

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>(ls.getLiveViewPixieScript());
  React.useEffect(() => {
    ls.setLiveViewPixieScript(script);
  }, [script]);

  const [vegaSpec, setVegaSpec] = React.useState<VisualizationSpecMap>(parseSpecs(ls.getLiveViewVegaSpec()) || {});

  const [placement, setPlacement] = React.useState<Placement>(parsePlacement(ls.getLiveViewPlacementSpec()) || {});

  const [tables, setTables] = React.useState<Tables>({});

  const setScripts = React.useCallback((newScript, newVega, newPlacement, newTitle) => {
    setScript(newScript);
    setTitle(newTitle);
    setVegaSpec(parseSpecs(newVega) || {});
    setPlacement(parsePlacement(newPlacement) || {});
  }, []);

  const [title, setTitle] = React.useState<Title>(ls.getLiveViewTitle());
  React.useEffect(() => {
    ls.setLiveViewTitle(title);
  }, [title]);

  const client = React.useContext(ClientContext);

  const showSnackbar = useSnackbar();

  const executeScript = React.useCallback((inputScript?: string) => {
    if (!client) {
      return;
    }
    let err;
    let queryId;
    client.executeScript(inputScript || script).then((results) => {
      const newTables = {};
      queryId = results.queryId;
      for (const table of results.tables) {
        newTables[table.name] = table;
      }
      setTables(newTables);
    }).catch((errMsg) => {
      err = errMsg;
      showSnackbar({
        message: 'Failed to execute script',
        action: () => executeScript(inputScript),
        actionTitle: 'retry',
        autoHideDuration: 5000,
      });
    }).finally(() => {
      analytics.track('Query Execution', {
        status: err ? 'success' : 'failed',
        query: script,
        queryID: queryId,
        error: err,
        title,
      });
    });
  }, [client, script, title]);

  const liveViewContext = React.useMemo(() => ({
    updateScript: setScript,
    updateVegaSpec: setVegaSpec,
    updatePlacement: setPlacement,
    vizierReady: !!client,
    setScripts,
    executeScript,
  }), [executeScript, client]);

  return (
    <LiveContext.Provider value={liveViewContext}>
      <TitleContext.Provider value={title}>
        <ScriptContext.Provider value={script}>
          <VegaContext.Provider value={vegaSpec}>
            <PlacementContext.Provider value={placement}>
              <ResultsContext.Provider value={tables}>
                {props.children}
              </ResultsContext.Provider>
            </PlacementContext.Provider>
          </VegaContext.Provider>
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
