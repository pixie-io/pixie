import * as storage from 'common/storage';
import * as React from 'react';
import urlParams from 'utils/url-params';

import { SetStateFunc } from './common';
import { RouteContext } from './route-context';
import { LiveViewPage } from '../utils/live-view-params';

interface ScriptContextProps {
  script: string;
  setScript: SetStateFunc<string>;
  title: string;
  id: string;
  setIdAndTitle: (id: string, title: string) => void;
}

export const ScriptContext = React.createContext<ScriptContextProps>(null);

export const ScriptContextProvider = (props) => {
  const [script, setScript] = storage.useSessionStorage(storage.LIVE_VIEW_PIXIE_SCRIPT_KEY, '');
  const [title, setTitle] = storage.useSessionStorage(storage.LIVE_VIEW_SCRIPT_TITLE_KEY, '');
  const [id, setId] = storage.useSessionStorage(storage.LIVE_VIEW_SCRIPT_ID_KEY, '');
  const {liveViewPage} = React.useContext(RouteContext);

  const setIdAndTitle = (newId: string, newTitle: string) => {
    setTitle(newTitle);
    setId(newId);
    if (liveViewPage === LiveViewPage.Default) {
      urlParams.setScript(newId, '' /* diff */);
    }
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
