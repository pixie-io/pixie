/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';
import { GetPxScripts, Script } from 'app/utils/script-bundle';
import { GQLUserInfo } from 'app/types/schema';
import { useQuery, gql } from '@apollo/client';

export interface ScriptsContextProps {
  scripts: Map<string, Script>;
  loading: boolean;
  setScratchScript: (Script) => void,
}

export const ScriptsContext = React.createContext<ScriptsContextProps>(null);

export const SCRATCH_SCRIPT: Script = {
  id: 'Scratch Pad',
  title: 'Scratch Pad',
  description: 'A clean slate for one-off scripts.\n'
    + 'This is ephemeral; it disappears upon changing scripts.',
  vis: {
    variables: [],
    widgets: [],
    globalFuncs: [],
  },
  code: 'import px\n\n'
    + '# Use this scratch pad to write and run one-off scripts.\n'
    + '# If you switch to another script, refresh, or close this browser tab, this script will disappear.\n\n',
  hidden: false,
};

export const ScriptsContextProvider: React.FC = ({ children }) => {
  const { data, loading, error } = useQuery<{
    user: Pick<GQLUserInfo, 'orgName' | 'orgID' >,
  }>(gql`
    query userOrgInfoForScripts{
      user {
        orgName
        orgID
      }
    }
  `, {});
  const user = data?.user;

  const [scripts, setScripts] = React.useState<Map<string, Script>>(new Map([['initial', {} as Script]]));
  const [scriptLoading, setScriptLoading] = React.useState<boolean>(true);

  React.useEffect(() => {
    if (!user || loading || error) return;
    GetPxScripts(user.orgID, user.orgName).then((list) => {
      const availableScripts = new Map<string, Script>(list.map((script) => [script.id, script]));
      const scratchScript = scripts.get(SCRATCH_SCRIPT.id) ?? SCRATCH_SCRIPT;
      availableScripts.set(SCRATCH_SCRIPT.id, scratchScript);
      setScripts(availableScripts);
      setScriptLoading(false);
    });
    // Monitoring the user's ID and their org rather than the whole object, to avoid excess renders.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [user?.orgID, user?.orgName, loading, error]);

  const setScratchScript = React.useCallback((script: Script) => {
    setScripts((currScripts) => {
      const newScripts = currScripts;
      newScripts.set(SCRATCH_SCRIPT.id, script);
      return newScripts;
    });
  }, []);

  const context = React.useMemo(() => (
    { scripts, loading: scriptLoading, setScratchScript }
  ), [scripts, scriptLoading, setScratchScript]);

  return (
    <ScriptsContext.Provider value={context}>
      {children}
    </ScriptsContext.Provider>
  );
};
