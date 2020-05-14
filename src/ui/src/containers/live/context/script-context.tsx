import * as ls from 'common/localstorage';
import * as React from 'react';

import { SetStateFunc } from './common';

interface ScriptContextProps {
  script: string;
  setScript: SetStateFunc<string>;
  title: string;
  id: string;
  setIdAndTitle: (id: string, title: string) => void;
}

export const ScriptContext = React.createContext<ScriptContextProps>(null);

export const ScriptContextProvider = (props) => {
  const [script, setScript] = ls.useLocalStorage(ls.LIVE_VIEW_PIXIE_SCRIPT_KEY, '');
  const [title, setTitle] = ls.useLocalStorage(ls.LIVE_VIEW_SCRIPT_TITLE_KEY, '');
  const [id, setId] = ls.useLocalStorage(ls.LIVE_VIEW_SCRIPT_ID_KEY, '');

  const setIdAndTitle = (newId: string, newTitle: string) => {
    setTitle(newTitle);
    setId(newId);
  };
  return (
    <ScriptContext.Provider value={{
      script,
      setScript,
      id,
      title,
      setIdAndTitle,
    }}>
      {props.children}
    </ScriptContext.Provider>
  );
};
