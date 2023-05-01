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

import { ScriptContext } from 'app/context/script-context';

import { CommandPaletteTrigger } from './command-palette-trigger';
import { quoteIfNeeded } from './providers/script/script-provider-common';

export const CommandPalette = React.memo(() => {
  const { script, args } = React.useContext(ScriptContext);

  // By default, fill the palette with a representation of the current live view breadcrumbs.
  // In other views, there's nothing to prefill.
  const text = React.useMemo(() => {
    if (!script?.id) return '';

    // We always want to show the script first, and the start_time last.
    const parts = [`script:${quoteIfNeeded(script.id)}`];
    const keys = [...Object.keys(args)];
    keys.sort((a, b) => +(a === 'start_time') - +(b === 'start_time'));

    // For each key, add ` key:value` to the string (quoting the value if it needs it)
    for (const k of keys) {
      const valRaw = Array.isArray(args[k]) ? (args[k] as string[]).join(',') : (args[k] as string);
      const v = quoteIfNeeded(valRaw);
      parts.push(`${k}:${v ?? ''}`);
    }
    return parts.join(' ');
  }, [script, args]);

  return <CommandPaletteTrigger text={text} />;
});
CommandPalette.displayName = 'CommandPalette';
