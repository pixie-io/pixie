import * as storage from 'common/storage';
import { parseVis, toJSON, Vis } from 'containers/live/vis';
import * as React from 'react';
import { debounce } from 'utils/debounce';

import { SetStateFunc } from './common';

interface VisContextProps {
  visJSON: string;
  vis: Vis;
  setVis: SetStateFunc<Vis>;
  setVisJSON: SetStateFunc<string>;
}

function emptyVis(): Vis {
  return { variables: [], widgets: [], globalFuncs: [] };
}

export const VisContext = React.createContext<VisContextProps>(null);

export const VisContextProvider = (props) => {
  const [visJSON, setVisJSONBase] = storage.useSessionStorage<string>('px-live-vis');
  const [vis, setVisBase] = React.useState(() => {
    const parsed = parseVis(visJSON);
    if (parsed) {
      return parsed;
    }
    return emptyVis();
  });

  const setVis = (newVis: Vis) => {
    if (!newVis) {
      newVis = emptyVis();
    }
    setVisJSONBase(toJSON(newVis));
    setVisBase(newVis);
  };

  const setVisDebounce = React.useRef(debounce((newJSON: string) => {
    const parsed = parseVis(newJSON);
    if (parsed) {
      setVisBase(parsed);
    }
  }, 2000));

  const setVisJSON = (newJSON: string) => {
    setVisJSONBase(newJSON);
    setVisDebounce.current(newJSON);
  };

  return (
    <VisContext.Provider value={{
      visJSON,
      vis,
      setVis,
      setVisJSON,
    }}>
      {props.children}
    </VisContext.Provider>
  );
};
