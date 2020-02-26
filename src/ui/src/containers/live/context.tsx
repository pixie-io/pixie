import {getClusterConnection} from 'common/cloud-gql-client';
import {
    getLiveViewPixieScript, getLiveViewVegaSpec, setLiveViewPixieScript, setLiveViewVegaSpec,
} from 'common/localstorage';
import {VizierGRPCClient, VizierQueryResult} from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
import * as React from 'react';

interface LiveContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (code: string) => void;
  vizierReady: boolean;
  executeScript: () => void;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<string>('');
export const ResultsContext = React.createContext<VizierQueryResult>(null);
export const LiveContext = React.createContext<LiveContextProps>(null);

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>(getLiveViewPixieScript());
  const [vegaSpec, setVegaSpec] = React.useState<string>(getLiveViewVegaSpec());
  const [results, setResults] = React.useState<VizierQueryResult>(null);

  const client = React.useContext(ClientContext);

  React.useEffect(() => {
    setLiveViewPixieScript(script);
  }, [script]);

  React.useEffect(() => {
    setLiveViewVegaSpec(vegaSpec);
  }, [vegaSpec]);

  const liveViewContext = React.useMemo(() => ({
    updateScript: setScript,
    updateVegaSpec: setVegaSpec,
    vizierReady: !!client,
    executeScript: () => {
      if (!client) {
        return;
      }
      client.executeScript(script).then(setResults);
    },
  }), [client, script]);

  return (
    <LiveContext.Provider value={liveViewContext}>
      <ScriptContext.Provider value={script}>
        <VegaContext.Provider value={vegaSpec}>
          <ResultsContext.Provider value={results}>
            {props.children}
          </ResultsContext.Provider>
        </VegaContext.Provider>
      </ScriptContext.Provider>
    </LiveContext.Provider>
  );
};

export default LiveContextProvider;
