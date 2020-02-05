import * as React from 'react';

// @ts-ignore : TS does not seem to like this import.
import * as demo from './demo.jxx';

interface ContextProps {
  updateScript: (code: string) => void;
  updateVegaSpec: (code: string) => void;
}

export const ScriptContext = React.createContext<string>('');
export const VegaContext = React.createContext<string>('');
export const LiveContext = React.createContext<Partial<ContextProps>>({});

const LiveContextProvider = (props) => {
  const [script, setScript] = React.useState<string>('');
  const [vegaSpec, setVegaSpec] = React.useState<string>(demo);

  const liveViewContext = React.useMemo(() => {
    const updateScript = (code: string) => { setScript(code); };
    const updateVegaSpec = (code: string) => { setVegaSpec(code); };
    return { updateScript, updateVegaSpec };
  }, []);

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
