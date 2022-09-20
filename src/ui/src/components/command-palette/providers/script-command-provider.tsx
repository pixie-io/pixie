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

import { gql } from '@apollo/client';
import {
  Badge as NamespaceIcon,
  DryCleaning as ServiceIcon,
  GridView as NodeIcon,
  Inventory as PodIcon,
} from '@mui/icons-material';
import { Box } from '@mui/material';

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { isPixieEmbedded } from 'app/common/embed-context';
import { PixieCommandIcon as ScriptIcon, PlayIcon } from 'app/components';
import { parse } from 'app/components/command-palette/parser';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import {
  GQLAutocompleteFieldResult,
  GQLAutocompleteEntityKind,
} from 'app/types/schema';
import { CancellablePromise, makeCancellable } from 'app/utils/cancellable-promise';
import { Script } from 'app/utils/script-bundle';
import { highlightMatch, normalize } from 'app/utils/string-search';

import { CommandCompletion, CommandProvider } from './command-provider';

// A hacky solution until we check by type rather than by arg name in a later change.
enum CommonArgNames {
  SCRIPT = 'script',
  POD = 'pod',
  SVC = 'service',
  NAMESPACE = 'namespace',
  NODE = 'node',
}

async function getFieldSuggestions(
  search: string,
  kind: GQLAutocompleteEntityKind,
  clusterUID: string,
  client: PixieAPIClient,
): Promise<GQLAutocompleteFieldResult> {
  const query = client.getCloudClient().graphQL.query<{ autocompleteField: GQLAutocompleteFieldResult }>({
    query: gql`
      query getCompletions($input: String, $kind: AutocompleteEntityKind, $clusterUID:String) {
        autocompleteField(input: $input, fieldType: $kind, clusterUID: $clusterUID) {
          suggestions {
            name
            description
            matchedIndexes
            state
          }
          hasAdditionalMatches
        }
      }
    `,
    fetchPolicy: 'no-cache',
    variables: {
      input: search,
      kind,
      clusterUID,
    },
  });

  try {
    const { data: { autocompleteField } } = await query;
    return autocompleteField;
  } catch (err) {
    console.warn('Something went wrong fetching suggestions, likely transient:', err);
    return { suggestions: [], hasAdditionalMatches: false };
  }
}

function getScriptSuggestions(partial: string, scripts: Map<string, Script>): GQLAutocompleteFieldResult {
  const scriptIds = [...scripts.keys()];
  const normalizedInput = normalize(partial);
  const normalizedScratchId = normalize(SCRATCH_SCRIPT.id);

  const ids = !partial ? [...scriptIds] : scriptIds.filter((s) => {
    const ns = normalize(s);
    return ns === normalizedScratchId || ns.indexOf(normalizedInput) >= 0;
  });

  // The `px` namespace should appear before all others
  ids.sort((a, b) => Number(b.startsWith('px/')) - Number(a.startsWith('px/')));

  // The scratch script should always appear at the top of the list for visibility. It doesn't get auto-selected
  // unless it's the only thing in the list.
  const scratchIndex = ids.indexOf(SCRATCH_SCRIPT.id);
  if (scratchIndex !== -1) {
    ids.splice(scratchIndex, 1);
    // Don't include SCRATCH in embedded views.
    if (!isPixieEmbedded()) {
      ids.unshift(SCRATCH_SCRIPT.id);
    }
  }

  const suggestions = ids.map((scriptId) => ({
    name: scriptId,
    description: scripts.get(scriptId).description,
    matchedIndexes: scriptId === SCRATCH_SCRIPT.id ? [] : highlightMatch(partial, scriptId),
  }));

  // Prefer closest matches
  suggestions.sort((a, b) => b.matchedIndexes.length - a.matchedIndexes.length);

  return {
    suggestions: suggestions.slice(0, 5),
    hasAdditionalMatches: suggestions.length > 5,
  };
}

const CompletionLabel = React.memo<{
  icon: React.ReactNode,
  input: string,
  highlights: number[],
}>(({ icon, input, highlights }) => {
  const outs: React.ReactNode[] = [];
  for (let i = 0; i < input.length; i++) {
    if (highlights.includes(i)) outs.push(<strong key={i}>{input.substring(i, i + 1)}</strong>);
    else outs.push(<span key={i}>{input.substring(i, i + 1)}</span>);
  }
  return (
    // eslint-disable-next-line react-memo/require-usememo
    <Box sx={{ display: 'flex', gap: 1, flexFlow: 'row nowrap' }}>
      {icon}
      <span>{outs}</span>
    </Box>
  );
});
CompletionLabel.displayName = 'CompletionLabel';

async function getAutocompleteSuggestions(
  input: string,
  selection: [start: number, end: number],
  clusterUID: string,
  client: PixieAPIClient,
  scripts: Map<string, Script>,
): Promise<{ completions: CommandCompletion[], hasAdditionalMatches: boolean }> {
  const parsed = parse(input, selection);

  const selectedKeys = parsed.selectedTokens.filter(t => t.token.type === 'key');
  const selectedEq = parsed.selectedTokens.filter(t => t.token.type === 'eq');
  const selectedValues = parsed.selectedTokens.filter(t => t.token.type === 'value');

  // Only suggest something if the selection covers exactly one significant token.
  if (selectedKeys.length + selectedValues.length > 1) {
    // TODO(nick): If the selection is just a bunch of `value`+`none` tokens, we could suggest as if they were combined.
    return { completions: [], hasAdditionalMatches: false };
  }

  // A key token sets `relatedToken` to the value token, if it exists. Both eq and value tokens link to their key token.
  const keyToken = selectedKeys[0]?.token
    ?? selectedValues[0]?.token.relatedToken
    ?? selectedEq[0]?.token.relatedToken
    ?? null;

  const valueToken = selectedValues[0]?.token
    ?? selectedKeys[0]?.token.relatedToken
    ?? selectedEq[0]?.token.relatedToken?.relatedToken
    ?? null;

  if (valueToken && !keyToken) {
    // TODO(nick): It's a bare value. We can suggest a key, or a key:value if the bare value resembles a known type.
    //   For example, `px/http` -> `script:px/http_data`; `script:px/pod p` -> `script:px/pod pod:`; `` -> `script:`
    //   GQLAutocompleteEntityKind.AEK_UNKNOWN might work here? If it reliably returns `kind`, that is.
    const existingKeys = new Set(...(parsed.kvMap?.keys() ?? []));
    existingKeys.delete(valueToken.value);

    if (parsed.kvMap?.get('script')?.length && valueToken.value !== 'script') {
      // TODO(nick): We already have a script; this is something else. Valid keys are the names of that script's args.
    }

    return {
      completions:[{
        key: 'bare-value',
        label: `Bare value: "${valueToken.text}"`,
        description: null,
        onSelect: () => {
          console.info('NYI: Selected bare-value, but there is no meaning for that yet');
        },
      }],
      hasAdditionalMatches: false,
    };
  } else if (keyToken) {
    // TODO(nick): If there's no script yet but the input looks like a pod (for example), suggest `script:px/pod pod:_`.
    let heading: string;
    let result: GQLAutocompleteFieldResult;
    let icon: React.ReactNode;
    switch (keyToken.value) {
      case CommonArgNames.SCRIPT:
        heading = 'PxL Scripts';
        icon = <ScriptIcon />;
        result = getScriptSuggestions(valueToken?.text || '', scripts);
        break;
      case CommonArgNames.POD:
        heading = 'Pods';
        icon = <PodIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_POD, clusterUID, client);
        break;
      case CommonArgNames.SVC:
        heading = 'Services';
        icon = <ServiceIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_SVC, clusterUID, client);
        break;
      case CommonArgNames.NAMESPACE:
        heading = 'Namespaces';
        icon = <NamespaceIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_NAMESPACE, clusterUID, client);
        break;
      case CommonArgNames.NODE:
        heading = 'Nodes';
        icon = <NodeIcon />;
        result = await getFieldSuggestions(
          valueToken?.text || '', GQLAutocompleteEntityKind.AEK_NODE, clusterUID, client);
        break;
      default:
        // TODO(nick): If `key` matches the script's arguments, check options for that arg. If not, maybe it's partial.
        return {
          completions: [{
            key: 'keyval',
            label: `Key/value: "${keyToken.text}":"${valueToken?.text ?? '(no value token present)'}"`,
            description: 'A key/value token pair',
            onSelect: () => {
              console.info('NYI: Selected option keyval, but there is no meaning for that yet');
            },
          }],
          hasAdditionalMatches: false,
        };
    }

    return {
      completions: result.suggestions.map((s, i) => ({
        heading,
        key: `${heading}_${s.name}_${i}`,
        label: <CompletionLabel icon={icon} input={s.name} highlights={s.matchedIndexes} />,
        description: s.description || `One ${heading}, "${s.name}"`,
        onSelect: () => {
          // Quote the value if needed
          const newName = /[\s"]+/.test(s.name) ? `"${s.name.replace(/"/g, '\\"')}"` : s.name;

          // Here, we do have a keyToken, and thus we must also have an eqToken. So, we're replacing the value.
          const splitIndex = parsed.tokens.findIndex((t) => t.type === 'eq' && t.relatedToken === keyToken);

          const earlierTokens = parsed.tokens.slice(0, splitIndex + 1);
          const laterTokens = parsed.tokens.slice(valueToken ? valueToken.index + 1 : splitIndex + 1);
          const prefix = earlierTokens.map(t => t.text).join('') + newName;
          return [
            prefix + laterTokens.map(t => t.text).join(''),
            prefix.length,
          ];
        },
      })),
      hasAdditionalMatches: result.hasAdditionalMatches,
    };
  } else if (input.length) {
    // Selection doesn't cover anything that makes sense to this suggester, and it wasn't the empty string.
    // Don't suggest anything in this case.
    return { completions: [], hasAdditionalMatches: false };
  } else {
    // Empty input. Suggest some common scripts.
    // TODO(nick): Suggest several common options here.
    return {
      completions: [{
        key: GQLAutocompleteEntityKind.AEK_SCRIPT,
        label: 'script:px/cluster',
        description: 'Read the high-level status of your cluster: namespaces, pods, performance metrics, etc.',
        onSelect: () => ['script:px/cluster start_time:-5m', 32],
      }],
      hasAdditionalMatches: false,
    };
  }
}

export const useScriptCommandProvider: CommandProvider = () => {
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;

  const clusterContext = React.useContext(ClusterContext);
  const clusterUID = clusterContext?.selectedClusterUID || null;

  const scriptsContext = React.useContext(ScriptsContext);
  const scripts = scriptsContext?.scripts;

  const [, setPromise] = React.useState<CancellablePromise<{
    completions: CommandCompletion[], hasAdditionalMatches: boolean }>>(null);

  return React.useCallback(async (input: string, selection: [start: number, end: number]) => {
    const parsed = parse(input, selection);
    const newPromise = makeCancellable(
      getAutocompleteSuggestions(input, selection, clusterUID, client, scripts ?? new Map()),
    );
    setPromise((prev) => {
      prev?.cancel();
      return newPromise;
    });

    const result = await newPromise;
    return {
      providerName: 'PxL',
      hasAdditionalMatches: result.hasAdditionalMatches,
      completions: result.completions,
      cta: {
        label: (
          // eslint-disable-next-line react-memo/require-usememo
          <Box sx={{ display: 'flex', gap: 1, flexFlow: 'row nowrap' }}>
            <PlayIcon />
            <strong>Run</strong>
          </Box>
        ),
        action: () => {
          alert('Not Yet Implemented. This CTA came from ScriptCommandProvider.');
        },
        disabled: !(parsed.kvMap?.get('script')?.length > 0),
        tooltip: 'NYI',
      },
    };
  }, [clusterUID, client, scripts]);
};
