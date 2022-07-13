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

import { useQuery, gql } from '@apollo/client';
import { Box, Button, Tooltip, Typography } from '@mui/material';

import { PixienautBox } from 'app/components';
import { GQLUserInfo } from 'app/types/schema';
import { makeCancellable } from 'app/utils/cancellable-promise';
import { WithChildren } from 'app/utils/react-boilerplate';
import { GetPxScripts, Script } from 'app/utils/script-bundle';

export interface ScriptsContextProps {
  scripts: Map<string, Script>;
  loading: boolean;
  error?: string;
  setScratchScript: (script: Script) => void,
}

export const ScriptsContext = React.createContext<ScriptsContextProps>(null);
ScriptsContext.displayName = 'ScriptsContext';

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

export const ScriptsContextProvider = React.memo<WithChildren>(({ children }) => {
  const { data, loading, error } = useQuery<{
    user: Pick<GQLUserInfo, 'orgName' | 'orgID' >,
  }>(gql`
    query userOrgInfoForScripts{
      user {
        id
        orgName
        orgID
      }
    }
  `, {});
  const user = data?.user;

  const [scripts, setScripts] = React.useState<Map<string, Script>>(new Map([['initial', {} as Script]]));
  const [scriptLoading, setScriptLoading] = React.useState<boolean>(true);
  const [scriptError, setScriptError] = React.useState<string>(null);

  React.useEffect(() => {
    if (!user || loading || error) return () => {};
    setScriptLoading(true);
    setScriptError(null);
    const promise = makeCancellable(GetPxScripts(user.orgID, user.orgName));
    promise
      .then((list) => {
        const availableScripts = new Map<string, Script>(list.map((script) => [script.id, script]));
        const scratchScript = scripts.get(SCRATCH_SCRIPT.id) ?? SCRATCH_SCRIPT;
        availableScripts.set(SCRATCH_SCRIPT.id, scratchScript);
        setScripts(availableScripts);
        setScriptLoading(false);
        setScriptError(null);
      })
      .catch((listError) => {
        console.error(listError);
        setScriptLoading(false);
        setScriptError(listError.message);
      });
    return () => promise.cancel();
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
    { scripts, loading: scriptLoading, error: scriptError, setScratchScript }
  ), [scripts, scriptLoading, scriptError, setScratchScript]);

  return (
    <ScriptsContext.Provider value={context}>
      {children}
    </ScriptsContext.Provider>
  );
});
ScriptsContextProvider.displayName = 'ScriptsContextProvider';

export const ScriptBundleErrorView = React.memo<{ reason: string }>(({ reason }) => {
  return (
    /* eslint-disable react-memo/require-usememo */
    <PixienautBox image='octopus'>
      <Typography variant='h1' sx={{ color: 'foreground.two' }}>
        Failed to Load Script Bundles
      </Typography>
      <Typography variant='body1' sx={{ color: 'foreground.one', mt: 4, mb: 4, maxWidth: 'sm' }}>
        Pixie failed to load PxL script bundles (<code>{reason}</code>).
        <br/><br/>
        If you are running Pixie in an air-gapped environment, or want to develop custom script bundles, follow
        the instructions below.
      </Typography>
      <Box sx={{
        p: 4,
        gap: ({ spacing }) => spacing(4),
        display: 'flex',
        flexFlow: 'row nowrap',
        justifyContent: 'space-around',
        alignItems: 'center',
        borderTop: ({ palette }) => `1px solid ${palette.foreground.grey1}`,
      }}>
        <Tooltip placement='bottom' title='Running Pixie in an air-gapped environment'>
          <Button
            variant='text'
            href='https://docs.px.dev/airgap'
            target='_blank'
          >
            Airgap
          </Button>
        </Tooltip>
        <Tooltip placement='bottom' title='Running a PxL script development environment'>
          <Button
            variant='text'
            href='https://docs.px.dev/tutorials/pxl-scripts/script-dev-environment'
            target='_blank'
          >
            Script Dev
          </Button>
        </Tooltip>
      </Box>
    </PixienautBox>
    /* eslint-enable react-memo/require-usememo */
  );
});
ScriptBundleErrorView.displayName = 'ScriptBundleErrorView';
