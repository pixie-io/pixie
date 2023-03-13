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

import { Box } from '@mui/material';

import { PlayIcon } from 'app/components';
import { CommandPaletteContext } from 'app/components/command-palette/command-palette-context';
import { CommandProvider } from 'app/components/command-palette/providers/command-provider';
import { ScriptsContext } from 'app/containers/App/scripts-context';
import { ScriptContext } from 'app/context/script-context';

import { CompletionDescription } from './script-provider-common';
import { getScriptCommandCta } from './script-provider-cta';

export const useScriptCommandIsValidProvider: CommandProvider = () => {
  const scripts = React.useContext(ScriptsContext)?.scripts;
  const { setScriptAndArgs } = React.useContext(ScriptContext);
  const { setOpen } = React.useContext(CommandPaletteContext);

  return React.useCallback(async (input: string, selection: [number, number]) => {
    const cta = getScriptCommandCta(input, selection, scripts, setScriptAndArgs, () => setOpen(false));
    const completions = (!cta || cta.disabled) ? [] : [{
      key: 'run-valid-script',
      label: (
        // eslint-disable-next-line react-memo/require-usememo
        <Box sx={{ display: 'flex', gap: 1, flexFlow: 'row nowrap' }}>
          <PlayIcon />
          <strong>Run This Script</strong>
        </Box>
      ),
      description: (
        <CompletionDescription title='Run Script Command' body='Script command is valid.' />
      ),
      onSelect: () => {
        if (cta.disabled === false) cta.action();
      }, // Doesn't change the input.
    }];

    return {
      providerName: 'Run Script',
      hasAdditionalMatches: false,
      completions,
      cta,
    };
  }, [scripts, setOpen, setScriptAndArgs]);
};
