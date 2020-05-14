import * as React from 'react';
import { getQueryParams } from 'utils/query-params';
import { GetPxScripts, Script } from 'utils/script-bundle';

interface ScriptsContextProps {
  scripts: Script[];
}

export const ScriptsContext = React.createContext<ScriptsContextProps>(null);

export const ScriptsContextProvider = (props) => {
  const [scripts, setScripts] = React.useState<Script[]>([]);
  React.useEffect(() => {
    const orgName = getQueryParams().org_name || '';
    GetPxScripts(orgName).then((pxScripts) => {
      // Fitler out the hidden scripts.
      setScripts(pxScripts.filter((s) => !s.hidden));
    });
  }, []);

  const context = React.useMemo(() => ({ scripts }), [scripts]);

  return (
    <ScriptsContext.Provider value={context}>
      {props.children}
    </ScriptsContext.Provider>
  );
};
