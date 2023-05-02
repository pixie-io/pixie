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
import { parse } from 'app/components/command-palette/parser';
import { CommandCta } from 'app/components/command-palette/providers/command-provider';
import { Script } from 'app/utils/script-bundle';

/**
 * Test if the input represents a valid invocation of a known PxL script. All required args present, no extra keys, etc.
 * @param kvMap The current key:value pairs in the input (like `script:foo`, `arg:val`)
 * @param scripts The known set of scripts available to use
 * @returns True if the input represents a valid invocation of a script in `scripts` (all args filled out correctly)
 */
export function isScriptCommandValid(kvMap: Map<string, string>, scripts: Map<string, Script>): boolean {
  const scriptId = kvMap.get('script')?.trim();
  if (!scriptId) return false;
  const script = scripts.get(scriptId);
  if (!script) return false;

  // Make sure that all args that need to exist do, and have allowed values
  for (const arg of script.vis.variables) {
    const val = kvMap.get(arg.name);
    const clearValidValues = arg.validValues?.includes(val) ?? true;
    const required = !arg.defaultValue && !arg.validValues?.length;
    if (!clearValidValues || !kvMap.has(arg.name) || (required && !kvMap.get(arg.name)?.trim().length)) return false;
  }

  // Extraneous keys are errors
  for (const k in kvMap) {
    if (k !== 'script' && !script.vis.variables.some(a => a.name === k)) return false;
  }

  return true;
}

/**
 * Common CTA for providers that suggest PxL scripts.
 *
 * @param input The text the user has typed so far
 * @param selection Where the caret (or selection) is
 * @returns The CTA common to all Script command providers, which validates the input and triggers a script run.
 */
export function getScriptCommandCta(
  input: string,
  selection: [start: number, end: number],
  scripts: Map<string, Script>,
  setScriptAndArgs: (script: Script, args: Record<string, string | string[]>) => void,
  closeFn: () => void,
): CommandCta {
  if (!input.length) return null;

  const parsed = parse(input, selection);
  const valid = isScriptCommandValid(parsed.kvMap ?? new Map(), scripts ?? new Map());

  let script: Script = null;
  const args: Record<string, string | string[]> = {};
  if (valid) {
    script = scripts.get(parsed.kvMap.get('script'));
    for (const arg of script.vis.variables) {
      args[arg.name] = parsed.kvMap.get(arg.name) ?? arg.defaultValue ?? '';
    }
  }

  return {
    label: (
      // eslint-disable-next-line react-memo/require-usememo
      <Box sx={{ display: 'flex', gap: 1, flexFlow: 'row nowrap' }}>
        <PlayIcon />
        <strong>Run</strong>
      </Box>
    ),
    action: () => {
      if (!valid || !script) return;
      // Closing the dialog first makes sure it visually goes away before the main thread focuses on running the script.
      // Otherwise, there's a noticeable delay before the dialog hides, which feels clunkier.
      closeFn();
      setScriptAndArgs(script, args);
    },
    disabled: !valid,
    tooltip: valid ? `Run "${script.id}" with these arguments` : 'Need a valid script command to run.',
  };
}
