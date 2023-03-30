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

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { ClusterIcon, NamespaceIcon, PodIcon, ServiceIcon } from 'app/components';
import { parse, ParseResult, Token } from 'app/components/command-palette/parser';
import {
  CommandProvider,
  CommandProviderResult,
} from 'app/components/command-palette/providers/command-provider';
import { ScriptsContext } from 'app/containers/App/scripts-context';
import { ScriptContext } from 'app/context/script-context';
import {
  GQLAutocompleteFieldResult,
  GQLAutocompleteEntityKind,
} from 'app/types/schema';
import { Script } from 'app/utils/script-bundle';

import {
  useMissingScriptProvider,
} from '.';
import { CommandPaletteContext } from '../../command-palette-context';
import {
  CompletionLabel,
  getFieldSuggestions,
  getOnSelectSetKeyVal,
  getSelectedKeyAndValueToken,
  quoteIfNeeded,
} from './script-provider-common';
import { getScriptCommandCta, isScriptCommandValid } from './script-provider-cta';
import {
  getFullScriptSuggestions,
  getScriptArgSuggestions,
  getScriptArgValueSuggestions,
} from './script-suggestions';


/**
 * Get suggestions for a bare value (not attached to a key token) somewhere in the current string.
 * This is the most complex suggester, as the possibilities depend on what else is already in the full input.
 *
 * For example, if the input is `script:px/http_data st` and the caret is at the end, `bare` is `st` here,
 * suggestions might include `script:px/http_data start_time:` (fill out the argument).
 * Alternatively, if there's an argument with set `validValues`, and `st` matches one of those, it might also suggest
 * adding that argument and setting its value to the matched one.
 *
 * But if the input is just `pod`, we might suggest `script:px/pod ...`, or any other script that takes `pod` as an arg,
 * or any script that takes a namespace arg where one of the namespaces happens to be named `pod`, and so on.
 */
async function suggestFromBare(
  parsed: ParseResult,
  selectedToken: Token,
  clusterUID: string,
  client: PixieAPIClient,
  scripts: Map<string, Script>,
): Promise<CommandProviderResult> {
  const script: Script | null = scripts.get(parsed.kvMap.get('script') ?? '') ?? null;

  // If there is already a script, its arguments all come into play.
  const argSuggestions = script
    ? await getScriptArgSuggestions(parsed, selectedToken, clusterUID, client, script)
    : { completions: [], hasAdditionalMatches: false };

  const valueSuggestions = script
    ? getScriptArgValueSuggestions(parsed, selectedToken, script)
    : { completions: [], hasAdditionalMatches: false };

  const hasAdditionalMatches = argSuggestions.hasAdditionalMatches || valueSuggestions.hasAdditionalMatches;

  return {
    providerName: 'Scripts',
    completions: [
      ...argSuggestions.completions,
      ...valueSuggestions.completions,
    ],
    hasAdditionalMatches,
  };
}

function useSuggestFromValue() {
  const clusterUID = React.useContext(ClusterContext)?.selectedClusterUID ?? null;
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  const scripts = React.useContext(ScriptsContext)?.scripts;
  return React.useCallback((input: string, selection: [number, number]) => {
    const parsed = parse(input, selection);
    let valueToken = getSelectedKeyAndValueToken(parsed).valueToken ?? null;
    if (!valueToken) valueToken = parsed.selectedTokens[0].token; // In case the selection was between two spaces
    return suggestFromBare(parsed, valueToken, clusterUID, client, scripts);
  }, [scripts, clusterUID, client]);
}

async function suggestFromKey(
  parsed: ParseResult,
  keyToken: Token,
  clusterUID: string,
  client: PixieAPIClient,
  scripts: Map<string, Script>,
): Promise<CommandProviderResult> {
  const valueToken = keyToken.relatedToken ?? null;

  let heading: string;
  let result: GQLAutocompleteFieldResult;
  let icon: React.ReactNode;

  if (keyToken.value === 'script') return getFullScriptSuggestions(valueToken?.text || '', scripts);

  const scriptId = parsed.kvMap?.get('script');
  if (scriptId && scripts.has(scriptId)) {
    // We do have a script, and this isn't the script. Use information about the script's args to issue suggestions.
    const script = scripts.get(scriptId);
    const foundArg = script.vis.variables.find(v => v.name === keyToken.value);
    if (!foundArg) return { providerName: 'Scripts', completions: [], hasAdditionalMatches: false };

    switch (foundArg.type) {
      case 'PX_POD':
        heading = 'Pods';
        icon = <PodIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_POD, clusterUID, client);
        break;
      case 'PX_SERVICE':
        heading = 'Services';
        icon = <ServiceIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_SVC, clusterUID, client);
        break;
      case 'PX_NAMESPACE':
        heading = 'Namespaces';
        icon = <NamespaceIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_NAMESPACE, clusterUID, client);
        break;
      case 'PX_NODE':
        heading = 'Nodes';
        icon = <ClusterIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_NODE, clusterUID, client);
        break;
      default:
        heading = 'MISSED THIS CASE';
        icon = <PodIcon />;
        result = { suggestions: [], hasAdditionalMatches: false };
        break;
    }
  } else {
    // This function only gets called if there is a script, so getting here means it isn't a known script.
    return { providerName: 'Scripts', completions: [], hasAdditionalMatches: false };
  }

  return {
    providerName: 'Scripts',
    completions: result.suggestions.map((s, i) => ({
      heading,
      key: `${heading}_${s.name}_${i}`,
      label: <CompletionLabel icon={icon} input={s.name} highlights={s.matchedIndexes} />,
      description: s.description || `One ${heading}, "${s.name}"`,
      onSelect: getOnSelectSetKeyVal(parsed, keyToken, keyToken.value, s.name),
    })),
    hasAdditionalMatches: result.hasAdditionalMatches,
  };
}

function useSuggestFromKeyedValue() {
  const clusterUID = React.useContext(ClusterContext)?.selectedClusterUID ?? null;
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  const scripts = React.useContext(ScriptsContext)?.scripts;
  return React.useCallback((input: string, selection: [number, number]) => {
    const parsed = parse(input, selection);
    const { keyToken } = getSelectedKeyAndValueToken(parsed);
    if (!keyToken) throw new Error('Called suggestFromKeyedValue, but there is no key token related to the selection');
    return suggestFromKey(parsed, keyToken, clusterUID, client, scripts);
  }, [client, clusterUID, scripts]);
}

/**
 * Provides PxL Script commands, such as `script:px/cluster start_time:-5m`, and understands the nuances involved to
 * suggest the parts of the command separately.
 *
 * Some examples of what it will suggest (the ‸ symbol represents the user's text caret or selection):
 * `` (empty input) -> `script:px/cluster start_time:-5m‸`, recommends some common commands
 * `script:px/cluster ‸` -> `script:px/cluster start_time:‸`, discovers missing arguments
 * `script:px/pod foo‸` -> `script:px/pod pod:foo_pod`, discovers missing arguments with hints
 * `script:po‸d` -> `script:px/pod pod:‸ start_time:-5m` finishes the script, prefills default arguments, and tabs to
 *   the first argument that the user needs to fill in. Then, it will suggest completions for that argument.
 * `pod:foo‸` -> `script:px/pod pod:foo‸`, finds scripts that take args in the input if the script isn't set yet
 * `foo‸` -> `script:px/foo ...`, `script:px/pod pod:...`, can suggest many things based on what the input matches here
 * `script:px/namespace namespace:foo‸` -> suggests namespaces directly since that arg for that script accepts them
 */
export const useScriptCommandProvider: CommandProvider = () => {
  const scripts = React.useContext(ScriptsContext)?.scripts;
  const { setScriptAndArgs } = React.useContext(ScriptContext);

  const { setOpen } = React.useContext(CommandPaletteContext);

  const missingScriptProvider = useMissingScriptProvider();
  const bareValueProvider = useSuggestFromValue();
  const keyedValueProvider = useSuggestFromKeyedValue();

  return React.useCallback(async (input, selection) => {
    const parsed = parse(input, selection);

    const { keyToken, valueToken } = getSelectedKeyAndValueToken(parsed);

    const selectionTooBroad = keyToken && valueToken && keyToken.relatedToken !== valueToken;
    const inputIsOneLongString = parsed.tokens.every(({ type }) => type === 'value' || type === 'none');
    const script = scripts?.get(parsed.kvMap?.get('script') ?? '') ?? null;
    const commandFullyFilled = isScriptCommandValid(parsed.kvMap ?? new Map(), scripts ?? new Map());

    const cta = getScriptCommandCta(input, selection, scripts, setScriptAndArgs, () => setOpen(false));
    if (!input.trim().length) {
      // Defer to emptyInputScriptProvider (defined elsewhere)
      return { providerName: 'Scripts', completions: [], hasAdditionalMatches: false, cta };
    } else if (inputIsOneLongString) {
      // If the entire input is just text, try treating it as one giant value token.
      const newInput = quoteIfNeeded(parsed.tokens.map(({ text }) => text).join(''));
      const res = await missingScriptProvider(newInput, [newInput.length, newInput.length]);
      return { ...res, cta };
    } else if (selectionTooBroad || (!keyToken && !valueToken && commandFullyFilled)) {
      // Either selection crosses multiple key:value pairs,
      // Or it doesn't cross any and the command is already fully valid (all required args exist and are valid)
      return { providerName: 'Scripts', completions: [], hasAdditionalMatches: false, cta };
    } else if (!script && (keyToken || valueToken)) {
      // We don't have a (valid) script, but do have some input. Try to suggest scripts that make sense with the input.
      const res = await missingScriptProvider(parsed.input, parsed.selection);
      return { ...res, cta };
    } else if (keyToken) {
      // There is a script, and we have either `key:` or `key:value`. This is the simplest scenario.
      const res = await keyedValueProvider(parsed.input, parsed.selection);
      return { ...res, cta };
    } else {
      // We have a bare string (not part of a key:value pair). This is the most complex scenario.
      // This also comes up if the caret is between two spaces, or between a space and the start/end of the input.
      const res = await bareValueProvider(parsed.input, parsed.selection);
      return { ...res, cta };
    }
  }, [
    bareValueProvider, missingScriptProvider, keyedValueProvider,
    scripts, setScriptAndArgs, setOpen,
  ]);
};
