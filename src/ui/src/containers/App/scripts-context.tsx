import * as React from 'react';
import { GetPxScripts, Script } from 'utils/script-bundle';
import { USER_QUERIES } from 'pixie-api';

import { useApolloClient } from '@apollo/client';

export interface ScriptsContextProps {
  scripts: Map<string, Script>;
  promise: Promise<Map<string, Script>>;
}

export const ScriptsContext = React.createContext<ScriptsContextProps>(null);

export const SCRATCH_SCRIPT: Script = {
  id: 'Scratch Pad',
  title: 'Scratch Pad',
  description: 'A clean slate for one-off scripts.\n'
    + 'This is ephemeral; it disappears upon changing scripts.',
  vis: '',
  code: 'import px\n\n'
    + '# Use this scratch pad to write and run one-off scripts.\n'
    + '# If you switch to another script, refresh, or close this browser tab, this script will disappear.\n\n',
  hidden: false,
};

export const ScriptsContextProvider = (props) => {
  const client = useApolloClient();
  const [scripts, setScripts] = React.useState<Map<string, Script>>(new Map([['initial', {} as Script]]));

  const promise = React.useMemo(() => client.query({ query: USER_QUERIES.GET_USER_INFO, fetchPolicy: 'network-only' })
    .then((result) => {
      const orgName = result?.data?.user.orgName;
      return GetPxScripts(orgName);
    })
    .then((scriptsList) => new Map<string, Script>(scriptsList.map((script) => [script.id, script]))), [client]);

  React.useEffect(() => {
    // Do this only once.
    promise.then((availableScripts) => {
      availableScripts.set(SCRATCH_SCRIPT.id, SCRATCH_SCRIPT);
      setScripts(availableScripts);
    });
  }, [promise]);

  const context = React.useMemo(() => ({ scripts, promise }), [scripts, promise]);

  return (
    <ScriptsContext.Provider value={context}>
      {props.children}
    </ScriptsContext.Provider>
  );
};
