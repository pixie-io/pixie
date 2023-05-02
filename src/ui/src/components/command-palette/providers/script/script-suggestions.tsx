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

import { SettingsInputComponent as ArgIcon } from '@mui/icons-material';

import { PixieAPIClient } from 'app/api';
import { isPixieEmbedded } from 'app/common/embed-context';
import { PixieCommandIcon as ScriptIcon } from 'app/components';
import { ParseResult, Token } from 'app/components/command-palette/parser';
import { CommandProviderState } from 'app/components/command-palette/providers/command-provider';
import { SCRATCH_SCRIPT } from 'app/containers/App/scripts-context';
import { pxTypeToEntityType } from 'app/containers/live/autocomplete-utils';
import { GQLAutocompleteEntityKind, GQLAutocompleteFieldResult, GQLAutocompleteSuggestion } from 'app/types/schema';
import { Script } from 'app/utils/script-bundle';
import { highlightScoredMatch, highlightNamespacedScoredMatch } from 'app/utils/string-search';

import {
  CompletionDescription,
  CompletionLabel,
  CompletionSet,
  getFieldSuggestions,
  getOnSelectSetKeyVal,
  quoteIfNeeded,
} from './script-provider-common';

export function getScriptIdSuggestions(partial: string, scripts: Map<string, Script>): GQLAutocompleteFieldResult {
  const scriptIds = [...scripts.keys()];

  const suggestions = scriptIds.map((id) => {
    const h = highlightNamespacedScoredMatch(partial, id, '/');
    return {
      name: id,
      description: scripts.get(id).description,
      matchedIndexes: partial && id !== SCRATCH_SCRIPT.id ? h.highlights : [],
      score: h.distance,
    };
  }).filter((s) => {
    if (s.name === SCRATCH_SCRIPT.id) return !isPixieEmbedded();
    return !partial || s.matchedIndexes.length > 0;
  });

  /*
   * Sort by the following proirities (first non-tie decides the order of two suggestions):
   * - Scratch script always goes first
   * - px/* scripts go before other namespaces
   * - Otherwise, the quality of the match is used (perfect match is better than substring is better than typo is...)
   *
   * Example: if partial is `service_e`, matches would be sorted like this:
   * [Scratch Pad, px/service_edge_stats, px/service_resource_usage, pxbeta/service_endpoint, pxbeta/service_endpoints]
   */
  suggestions.sort((a, b) => {
    const isScratchDistance = Number(b.name === SCRATCH_SCRIPT.id) - Number(a.name === SCRATCH_SCRIPT.id);
    const namespaceDistance = Number(b.name.startsWith('px/')) - Number(a.name.startsWith('px/'));
    const relevanceDistance = a.score - b.score; // Lower scores are closer matches

    // Combine the scores by priority to sort
    return (isScratchDistance * 1e3)
    + (namespaceDistance * 1e2)
    + relevanceDistance;
  });

  return {
    suggestions: suggestions.slice(0, 25),
    hasAdditionalMatches: suggestions.length > 25,
  };
}

/**
 * Suggestions that replace the entire input with a ready-to-fill script command.
 * For example, `unknown:args will:be_lost px/http|` -> `script:px/http_data source_filter:| destination_filter: ...`
 * @param partial Ony the token that's being considered, so in `unknown:args px/http|` only the `px/http` part.
 * @param scripts All of the currently-loaded PxL scripts to consider
 * @param focusArg If set, and an arg with that name appears in the replaced input, the caret will end up there.
 */
export function getFullScriptSuggestions(
  partial: string,
  scripts: Map<string, Script>,
  focusArg?: string,
): CommandProviderState {
  const idSuggestions = getScriptIdSuggestions(partial, scripts);
  return {
    input: partial,
    selection: [0, 0],
    providerName: 'getFullScriptSuggestions',
    loading: false,
    completions: idSuggestions.suggestions.map(({ name, description, matchedIndexes }, i) => {
      return {
        heading: 'Scripts',
        key: `pxl_${name}_${i}`,
        // eslint-disable-next-line react-memo/require-usememo
        label: <CompletionLabel icon={<ScriptIcon />} input={name} highlights={matchedIndexes} />,
        description: (
          <>
            <h3>PxL Script: <strong>{name}</strong></h3>
            <p>{description}</p>
          </>
        ),
        onSelect: () => {
          const script = scripts.get(name);

          const argNames = script.vis.variables.map(v => v.name);
          argNames.sort((a, b) => +(a === 'start_time') - +(b === 'start_time'));

          // Note: we don't try to retain existing args in the input that happen to line up here.
          // Even args like `start_time` might not have the same reasonable range between two scripts, so we use the
          // default value (if available) instead of what might have already been in the input.
          let out = `script:${quoteIfNeeded(name)}`;
          let caret = 0;
          for (const arg of argNames) {
            const val = script.vis.variables.find((a) => a.name === arg).defaultValue ?? '';
            out += ` ${arg}:${quoteIfNeeded(val)}`;
            if (!caret || arg === focusArg) caret = out.length;
          }

          return [out, caret];
        },
      };
    }),
    hasAdditionalMatches: idSuggestions.hasAdditionalMatches,
  };
}

/**
 * Shortcut function to check all common entity types for partial completions at the same time.
 */
export async function getAllFieldSuggestions(
  partial: string,
  clusterUID: string,
  client: PixieAPIClient,
): Promise<Record<'namespaces' | 'nodes' | 'pods' | 'services', GQLAutocompleteFieldResult>> {
  const promises = [
    GQLAutocompleteEntityKind.AEK_NAMESPACE,
    GQLAutocompleteEntityKind.AEK_NODE,
    GQLAutocompleteEntityKind.AEK_POD,
    GQLAutocompleteEntityKind.AEK_SVC,
  ].map((kind) => getFieldSuggestions(partial, kind, clusterUID, client));
  const [namespaceRes, nodeRes, podRes, serviceRes] = await Promise.allSettled(promises);

  const [namespaces, nodes, pods, services] = [namespaceRes, nodeRes, podRes, serviceRes]
    .map((res) => res.status === 'fulfilled' ? res.value : { suggestions: [], hasAdditionalMatches: false });

  return { namespaces, nodes, pods, services };
}

/**
 * Given a list of scripts that may match and a list of already-searched entity suggestions for a partial input,
 * get a map of script IDs to a map of arg IDs to those entity suggestions.
 *
 * @param scripts Any scripts that the entity search applies to (may be just one if the input already had one selected)
 * @param results Entity suggestions as extracted from get[All]FieldSuggestions
 * @returns Map of (script.id -> (arg.id -> suggestion))
 */
export function getSuggestedArgsFromPossibleEntity(
  scripts: Script[], // May be as little as one script (if it's already locked in), logic applies the same either way
  results: Record<'namespaces' | 'nodes' | 'pods' | 'services', GQLAutocompleteFieldResult>,
): Record<string, Record<string, GQLAutocompleteSuggestion[]>> {
  const matched = {};
  const entityType = {
    namespaces: GQLAutocompleteEntityKind.AEK_NAMESPACE,
    nodes: GQLAutocompleteEntityKind.AEK_NODE,
    pods: GQLAutocompleteEntityKind.AEK_POD,
    services: GQLAutocompleteEntityKind.AEK_SVC,
  };
  for (const script of scripts) {
    const matchArgs = [];
    for (const [which, result] of Object.entries(results)) {
      if (!result.suggestions.length) continue;
      matchArgs.push(...script.vis.variables
        .filter(v => pxTypeToEntityType(v.type) === entityType[which])
        .map(v => ({ variable: v.name, suggestions: result.suggestions })));
    }
    if (matchArgs.length) {
      matched[script.id] = matchArgs.reduce((a, c) => ({ ...a, [c.variable]: c.suggestions }), {});
    }
  }
  return matched;
}

/**
 * Find script arg names (not values) from the selected token. That is, suggests the `key:` part for the given script.
 * @param parsed Fully parsed input
 * @param selectedToken Which token to use for suggestion search
 * @param script The script whose arguents should be searched
 * @returns A presentable completion list
 */
export async function getScriptArgSuggestions(
  parsed: ParseResult,
  selectedToken: Token,
  clusterUID: string,
  client: PixieAPIClient,
  script: Script,
): Promise<CompletionSet> {
  const partial = selectedToken.text.trim();
  const completions = [];
  // Look only at args that match the input, and pre-sort them by relevance
  const argsMatchedOnName = script.vis.variables
    .map((arg) => {
      const h = highlightScoredMatch(partial, arg.name);
      return {
        ...arg,
        highlights: h.highlights,
        score: h.distance,
      };
    })
    .filter((a) => !partial.length || a.highlights.length > 0);
  argsMatchedOnName.sort((a, b) => b.score - a.score);

  for (const arg of argsMatchedOnName) {
    if (!parsed.kvMap.has(arg.name)) {
      // There's a missing argument, so add that to the keys we could suggest
      completions.push({
        heading: `Arguments to ${script.id}`,
        key: arg.name,
        description: (
          <CompletionDescription title={arg.name} body={arg.description} hint={`Argument of ${script.id}`} />
        ),
        label: (
          <CompletionLabel
            input={`${arg.name}...`}
            // eslint-disable-next-line react-memo/require-usememo
            icon={<ArgIcon />}
            highlights={arg.highlights}
          />
        ),
        onSelect: getOnSelectSetKeyVal(parsed, selectedToken, arg.name, ''),
      });
    }
  }

  const possibleEntities = getSuggestedArgsFromPossibleEntity(
    [script],
    await getAllFieldSuggestions(partial, clusterUID, client));
  for (const argName in possibleEntities[script.id]) {
    const suggestions = possibleEntities[script.id][argName];
    const arg = script.vis.variables.find(v => v.name === argName);
    // Only add this if there was actually some search text, and it doesn't happen to match the arg's name
    // Otherwise, a duplicate (from the user's perspective) can occur.
    if (suggestions.length && partial.length && !completions.some(c => c.key === argName)) {
      completions.push({
        heading: `Arguments to ${script.id}`,
        key: argName + '_ent',
        description: (
          <CompletionDescription title={argName} body={arg.description} hint={`Argument of ${script.id}`} />
        ),
        label: (
          // eslint-disable-next-line react-memo/require-usememo
          <CompletionLabel input={`${argName}...`} icon={<ArgIcon />} highlights={[]} />
        ),
        onSelect: getOnSelectSetKeyVal(parsed, selectedToken, argName, partial, false),
      });
    }
  }

  return {
    completions: completions.slice(0, 25),
    hasAdditionalMatches: completions.length > 25,
  };
}

/**
 * Find script arg values (not names) from the selected token. That is, suggests the `:value` part for the given arg.
 * @param parsed Fully parsed input
 * @param selectedToken Which token to use for suggestion search
 * @param script The script whose arguents should be searched
 * @returns A presentable completion list
 */
export function getScriptArgValueSuggestions(
  parsed: ParseResult,
  selectedToken: Token,
  script: Script,
): CompletionSet {
  const partial = selectedToken?.type === 'value' ? selectedToken?.text.trim() ?? '' : '';
  const completions = [];

  const keyToken = [selectedToken, selectedToken?.relatedToken].find(t => t?.type === 'key') ?? null;
  const checkKey = keyToken?.value ?? '';

  const arg = script.vis.variables.find(v => checkKey.length > 0 && v.name === checkKey);
  if (arg) {
    // Note: this function _doesn't_ check for entities, because getScriptArgSuggestions and suggestFromBare already do.
    const values = (arg.validValues?.length ? arg.validValues : [partial])
      .map(v => ({ value: v, highlights: highlightScoredMatch(partial, v) }));

    values.sort((a, b) => a.highlights.distance - b.highlights.distance);

    completions.push(...values.map((v, i) => ({
      heading: `Valid values for "${arg.name}"`,
      key: String(i),
      description: arg.description,
      label: (
        <CompletionLabel
          input={v.value}
          // eslint-disable-next-line react-memo/require-usememo
          icon={<ArgIcon />}
          highlights={v.highlights.highlights}
        />
      ),
      onSelect: getOnSelectSetKeyVal(parsed, selectedToken, arg.name, v.value),
    })));
  }

  return {
    completions: completions.slice(0, 25),
    hasAdditionalMatches: completions.length > 25,
  };
}
