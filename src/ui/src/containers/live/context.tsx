import {getClusterConnection} from 'common/cloud-gql-client';
import {VizierGRPCClient, VizierQueryResult} from 'common/vizier-grpc-client';
import * as React from 'react';

// @ts-ignore : TS does not seem to like this import.
import * as demo from './demo.jxx';

interface LiveContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (code: string) => void;
  vizierReady: boolean;
  executeScript: () => void;
  results?: VizierQueryResult;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<string>('');
export const LiveContext = React.createContext<Partial<LiveContextProps>>({});

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>('');
  const [vegaSpec, setVegaSpec] = React.useState<string>(demo);
  const [client, setClient] = React.useState<VizierGRPCClient>(null);
  const [results, setResults] = React.useState<VizierQueryResult>(null);

  React.useEffect(() => {
    getClusterConnection().then(({ ipAddress, token }) => {
      setClient(new VizierGRPCClient(ipAddress, token));
    });
  }, []);

  const liveViewContext = React.useMemo(() => ({
    updateScript: setScript,
    updateVegaScript: setVegaSpec,
    vizierReady: !!client,
    executeScript: () => {
      if (!client) {
        return;
      }
      client.executeScript(script).then(setResults);
    },
    results,
  }), [client, script, results]);
  return (
    <LiveContext.Provider value={liveViewContext}>
      <ScriptContext.Provider value={script}>
        <VegaContext.Provider value={vegaSpec}>
          {props.children}
        </VegaContext.Provider>
      </ScriptContext.Provider>
    </LiveContext.Provider>
  );
};

export default LiveContextProvider;
