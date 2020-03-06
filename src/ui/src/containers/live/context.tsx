import * as ls from 'common/localstorage';
import ClientContext from 'common/vizier-grpc-client-context';
import {parseSpecs, VisualizationSpecMap} from 'components/vega/spec';
import * as React from 'react';
import * as toml from 'toml';
import {debounce} from 'utils/debounce';
import {dataFromProto} from 'utils/result-data-utils';

import {parsePlacement, Placement} from './layout';
// @ts-ignore : TS does not seem to like this import.
import * as Preset from './preset.toml';

interface LiveContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (spec: VisualizationSpecMap) => void;
  updatePlacement: (placement: Placement) => void;
  vizierReady: boolean;
  executeScript: () => void;
  resetScripts: () => void;
}

interface Tables {
  [name: string]: Array<{}>;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<VisualizationSpecMap>(null);
export const PlacementContext = React.createContext<Placement>(null);
export const ResultsContext = React.createContext<Tables>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);

const preset = toml.parse(Preset);

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>(ls.getLiveViewPixieScript());
  React.useEffect(() => {
    ls.setLiveViewPixieScript(script);
  }, [script]);

  const [vegaSpec, setVegaSpec] = React.useState<VisualizationSpecMap>(
    parseSpecs(ls.getLiveViewVegaSpec()) || {});
  const setVegaSpecDebounced = React.useCallback(debounce(setVegaSpec, 2000), []);
  React.useEffect(() => {
    ls.setLiveViewVegaSpec(JSON.stringify(vegaSpec, null, 2));
  }, [vegaSpec]);

  const [placement, setPlacement] = React.useState<Placement>(
    parsePlacement(ls.getLiveViewPlacementSpec()) || {});
  const setPlacementDebounced = React.useCallback(debounce(setPlacement, 2000), []);
  React.useEffect(() => {
    ls.setLiveViewPlacementSpec(JSON.stringify(placement, null, 2));
  }, [placement]);

  const [tables, setTables] = React.useState<Tables>({});

  const resetScripts = React.useCallback(() => {
    setScript(preset.script);
    setVegaSpec(parseSpecs(preset.vega));
    setPlacementDebounced(parsePlacement(preset.placement));
  }, []);

  const client = React.useContext(ClientContext);

  const liveViewContext = React.useMemo(() => ({
    updateScript: setScript,
    updateVegaSpec: setVegaSpecDebounced,
    updatePlacement: setPlacementDebounced,
    vizierReady: !!client,
    resetScripts,
    executeScript: () => {
      if (!client) {
        return;
      }
      client.executeScript(script).then((results) => {
        const newTables = {};
        for (const table of results.tables) {
          newTables[table.name] = dataFromProto(table.relation, table.data);
        }
        setTables(newTables);
      });
    },
  }), [client, script]);

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
