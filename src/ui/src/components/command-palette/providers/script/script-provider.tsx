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
  CommandProviderDispatchAction,
  CommandProviderState,
} from 'app/components/command-palette/providers/command-provider';
import { ScriptsContext } from 'app/containers/App/scripts-context';
import { ScriptContext } from 'app/context/script-context';
import {
  GQLAutocompleteFieldResult,
  GQLAutocompleteEntityKind,
} from 'app/types/schema';
import { CancellablePromise, makeCancellable } from 'app/utils/cancellable-promise';
import { checkExhaustive } from 'app/utils/check-exhaustive';
import { Script } from 'app/utils/script-bundle';

import {
  CompletionLabel,
  getFieldSuggestions,
  getOnSelectSetKeyVal,
  getSelectedKeyAndValueToken,
} from './script-provider-common';
import { getScriptCommandCta, isScriptCommandValid } from './script-provider-cta';
import {
  getFullScriptSuggestions,
  getScriptArgSuggestions,
  getScriptArgValueSuggestions,
} from './script-suggestions';
import { CommandPaletteContext } from '../../command-palette-context';

const DEFAULT: CommandProviderState = Object.freeze({
  input: '',
  selection: [0, 0] as [start: number, end: number],
  providerName: 'ScriptProvider',
  loading: false,
  completions: [],
  hasAdditionalMatches: false,
});

function useBaseProvider(
  suggester: (input: string, selection: [number, number]) => Promise<CommandProviderState>,
): ReturnType<CommandProvider> {
  const [promise, setPromise] = React.useState<CancellablePromise<Partial<CommandProviderState>>>(null);
  const [state, setState] = React.useState(DEFAULT);
  const dispatch = React.useCallback((action: CommandProviderDispatchAction) => {
    const { type } = action;
    switch (type) {
      case 'cancel':
        promise?.cancel();
        setPromise(null);
        setState((prev) => ({ ...prev, loading: false }));
        break;
      case 'invoke': {
        promise?.cancel();
        setState(DEFAULT);
        const p = makeCancellable(suggester(action.input, action.selection));
        setPromise(p);
        p.then((res) => {
          setState({ ...DEFAULT, ...res });
        });
        break;
      }
      default: checkExhaustive(type);
    }
  }, [promise, suggester]);

  return [state, dispatch];
}

/** @see {@link useScriptCommandFromKeyedValueProvider} */
async function suggestFromKey(
  parsed: ParseResult,
  keyToken: Token,
  clusterUID: string,
  client: PixieAPIClient,
  scripts: Map<string, Script>,
): Promise<Partial<CommandProviderState>> {
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
        if (foundArg.validValues?.length) {
          const { completions, hasAdditionalMatches } = getScriptArgValueSuggestions(
            parsed, valueToken ?? keyToken, script);
          return { providerName: 'Scripts', completions, hasAdditionalMatches };
        } else {
          heading = 'Unmatched';
          icon = <PodIcon />;
          result = { suggestions: [], hasAdditionalMatches: false };
        }
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

/**
 * Get suggestions for a key:value pair somewhere in the current string. Also works on `key:` (no value).
 *
 * For example, if the caret is at the end of the following inputs...
 * `script:pod` -> Look for script with `pod` in the name
 * `script:px/pod pod:foo` -> Look for Pod entities, since that argument to `px/pod` only takes those entities.
 * `script:px/node groupby:` -> The `groupby` argument has a strict set of `validValues`, so look through only those.
 */
export const useScriptCommandFromKeyedValueProvider: CommandProvider = () => {
  const clusterUID = React.useContext(ClusterContext)?.selectedClusterUID ?? null;
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  const scripts = React.useContext(ScriptsContext)?.scripts;
  const { setScriptAndArgs } = React.useContext(ScriptContext);
  const { setOpen } = React.useContext(CommandPaletteContext);

  const suggester = React.useCallback(async (input: string, selection: [number, number]) => {
    const parsed = parse(input, selection);
    const cta = getScriptCommandCta(input, selection, scripts, setScriptAndArgs, () => setOpen(false));
    const { keyToken, valueToken } = getSelectedKeyAndValueToken(parsed);

    const selectionTooBroad = keyToken && valueToken && keyToken.relatedToken !== valueToken;
    const inputIsOneLongString = parsed.tokens.every(({ type }) => type === 'value' || type === 'none');
    const script = scripts?.get(parsed.kvMap?.get('script') ?? '') ?? null;

    if (
      !input.trim().length
      || inputIsOneLongString
      || selectionTooBroad
      || !keyToken
      || (!script && valueToken)
    ) {
      return { ...DEFAULT, cta };
    }
    return { ...DEFAULT, ...(await suggestFromKey(parsed, keyToken, clusterUID, client, scripts)), cta };
  }, [client, clusterUID, scripts, setOpen, setScriptAndArgs]);

  return useBaseProvider(suggester);
};

/** @see {@link useScriptCommandFromValueProvider} */
async function suggestFromBare(
  parsed: ParseResult,
  selectedToken: Token,
  clusterUID: string,
  client: PixieAPIClient,
  scripts: Map<string, Script>,
): Promise<Partial<CommandProviderState>> {
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
export const useScriptCommandFromValueProvider: CommandProvider = () => {
  const clusterUID = React.useContext(ClusterContext)?.selectedClusterUID ?? null;
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  const scripts = React.useContext(ScriptsContext)?.scripts;
  const { setScriptAndArgs } = React.useContext(ScriptContext);
  const { setOpen } = React.useContext(CommandPaletteContext);

  const suggester = React.useCallback(async (input: string, selection: [number, number]) => {
    const parsed = parse(input, selection);
    const cta = getScriptCommandCta(input, selection, scripts, setScriptAndArgs, () => setOpen(false));
    const sel = getSelectedKeyAndValueToken(parsed);

    // In case selection was between two spaces, grab the nearest token (will be null on empty input)
    if (!sel.valueToken) sel.valueToken = parsed.selectedTokens[0]?.token ?? null;
    const { keyToken, valueToken } = sel;

    const selectionTooBroad = keyToken && valueToken && keyToken.relatedToken !== valueToken;
    const inputIsOneLongString = parsed.tokens.every(({ type }) => type === 'value' || type === 'none');
    const script = scripts?.get(parsed.kvMap?.get('script') ?? '') ?? null;
    const commandFullyFilled = isScriptCommandValid(parsed.kvMap ?? new Map(), scripts ?? new Map());

    if (
      !input.trim().length
      || !script
      || inputIsOneLongString
      || selectionTooBroad
      || keyToken
      || !valueToken
      || commandFullyFilled
    ) {
      return { ...DEFAULT, cta };
    }

    return { ...DEFAULT, ...(await suggestFromBare(parsed, valueToken, clusterUID, client, scripts)), cta };
  }, [scripts, setScriptAndArgs, clusterUID, client, setOpen]);

  return useBaseProvider(suggester);
};
