import * as ls from 'common/localstorage';
import {Table} from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import {useSnackbar} from 'components/snackbar/snackbar';
import {parseSpecs, VisualizationSpecMap} from 'components/vega/spec';
import * as React from 'react';
import {resolveTypeReferenceDirective} from 'typescript';

import {parsePlacement, Placement} from './layout';

interface LiveContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (spec: VisualizationSpecMap) => void;
  updatePlacement: (placement: Placement) => void;
  vizierReady: boolean;
  executeScript: () => void;
  setScripts: (script: string, vega: string, placement: string) => void;
}

interface Tables {
  [name: string]: Table;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<VisualizationSpecMap>(null);
export const PlacementContext = React.createContext<Placement>(null);
export const ResultsContext = React.createContext<Tables>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>(ls.getLiveViewPixieScript());
  React.useEffect(() => {
    ls.setLiveViewPixieScript(script);
  }, [script]);

  const [vegaSpec, setVegaSpec] = React.useState<VisualizationSpecMap>(
    parseSpecs(ls.getLiveViewVegaSpec()) || {});

  const [placement, setPlacement] = React.useState<Placement>(
    parsePlacement(ls.getLiveViewPlacementSpec()) || {});

  const [tables, setTables] = React.useState<Tables>({});

  const setScripts = React.useCallback((newScript, newVega, newPlacement) => {
    setScript(newScript);
    setVegaSpec(parseSpecs(newVega) || {});
    setPlacement(parsePlacement(newPlacement) || {});
  }, []);

  const client = React.useContext(ClientContext);

  const showSnackbar = useSnackbar();

  const executeScript = React.useCallback(() => {
    if (!client) {
      return;
    }
    client.executeScript(script).then((results) => {
      const newTables = {};
      for (const table of results.tables) {
        newTables[table.name] = table;
      }
      setTables(newTables);
    }).catch(() => {
      showSnackbar({
        message: 'Failed to execute script',
        action: executeScript,
        actionTitle: 'retry',
        autoHideDuration: 5000,
      });
    });
  }, [client, script]);

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
      <ScriptContext.Provider value={script}>
        <VegaContext.Provider value={vegaSpec}>
          <PlacementContext.Provider value={placement}>
            <ResultsContext.Provider value={tables}>
              {props.children}
            </ResultsContext.Provider>
          </PlacementContext.Provider>
        </VegaContext.Provider>
      </ScriptContext.Provider>
    </LiveContext.Provider>
  );
};

export default LiveContextProvider;
