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

import { PixieAPIContext, PixieAPIClient } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { PixieCommandIcon as ScriptIcon } from 'app/components';
import { parse } from 'app/components/command-palette/parser';
import { CommandProvider } from 'app/components/command-palette/providers/command-provider';
import { ScriptsContext } from 'app/containers/App/scripts-context';
import { highlightMatch } from 'app/utils/string-search';

import {
  CompletionDescription,
  CompletionLabel,
  getSelectedKeyAndValueToken,
  getStringHighlightSortFn,
  quoteIfNeeded,
} from './script-provider-common';
import {
  getAllFieldSuggestions,
  getFullScriptSuggestions,
  getSuggestedArgsFromPossibleEntity,
} from './script-suggestions';

/**
 * If the input doesn't currently have a `script:something` pair, try suggesting scripts that make sense with the input.
 * For instance, if the input is `pod:asdf`, only scripts who have a `pod` argument would be suggested.
 * Or, if the input is a partial match for some pod's name, only scripts with an arg of entity type `POD`.
 */
export const useMissingScriptProvider: CommandProvider = () => {
  const clusterUID = React.useContext(ClusterContext)?.selectedClusterUID ?? null;
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  const scripts = React.useContext(ScriptsContext)?.scripts;

  return React.useCallback(async (input, selection) => {
    const noMatchesOutput = { providerName: 'PxL Scripts', completions: [], hasAdditionalMatches: false };

    const parsed = parse(input, selection);
    const hasScriptKey = parsed.kvMap?.has('script');
    const script = scripts?.get(parsed.kvMap?.get('script') ?? '') ?? null;

    // This provider is only relevant if there is NOT a known script selected yet, and there are scripts to find.
    if (script || !scripts) return noMatchesOutput;

    const { keyToken, valueToken } = getSelectedKeyAndValueToken(parsed);

    // We're filling the script arg itself, skip all remaining logic
    if (keyToken?.value === 'script') {
      const partial = valueToken?.value ?? '';
      return {
        providerName: 'PxL Scripts',
        ...getFullScriptSuggestions(partial, scripts),
      };
    }

    let possibleScripts = [...scripts.values()];

    // If there's a script:something but it doesn't match a known script yet, filter the possible matches based on that
    if (hasScriptKey && !script && keyToken?.value !== 'script') {
      const partial = parsed.kvMap.get('script');
      possibleScripts = possibleScripts.filter(({ id }) => highlightMatch(partial, id).length > 0);
    }

    // Now, look for scripts in the remaining options that have at least one of the args present in the input.
    // If there aren't any (like if there aren't any args in the input yet), skip this step.
    const presentKeys = [...(parsed.kvMap?.keys() ?? [])].filter(k => k !== 'script');
    if (valueToken?.value && !keyToken) presentKeys.push(valueToken.value);

    const completionsFromArgMatch = [];
    for (const argScript of possibleScripts) {
      const candidates = argScript.vis.variables.map(v => v.name);
      const matches = candidates.map(c => {
        for (const p of presentKeys) {
          const h = highlightMatch(p, c);
          if (!h.length) return null;
          return { arg: c, search: p, highlight: h };
        }
      }).filter(m => m);
      if (!matches.length) continue; // Only looking for scripts where at least one arg matches

      matches.sort((ma, mb) => mb.highlight.length - ma.highlight.length);
      const goToArg = matches[0];
      const adjustedHighlight = goToArg.highlight.map(v => v + `${argScript.id} (`.length);
      completionsFromArgMatch.push({
        heading: 'Scripts',
        key: `missing_script_entity_${argScript.id}`,
        description: (
          <CompletionDescription
            title={argScript.id}
            body={argScript.description}
            hint={`matched on argument: ${goToArg.arg}`}
          />
        ),
        label: (
          <CompletionLabel
            input={`${argScript.id} (${goToArg.arg})`}
            // eslint-disable-next-line react-memo/require-usememo
            icon={<ScriptIcon />}
            highlights={adjustedHighlight}
          />
        ),
        onSelect: () => {
          const argNames = argScript.vis.variables.map(v => v.name);
          argNames.sort((a, b) => +(a === 'start_time') - +(b === 'start_time'));

          let out = `script:${quoteIfNeeded(argScript.id)}`;
          let caret = 0;
          for (const arg of argNames) {
            const dflt = argScript.vis.variables.find(a => a.name === arg).defaultValue ?? '';
            const val = parsed.kvMap?.get(arg) ?? /* TODO: Consider the bare value that partially matched here */ dflt;
            out += ` ${arg}:${quoteIfNeeded(val)}`;
            if (!caret || arg === goToArg.arg) caret = out.length;
          }

          return [out, caret];
        },
      });
    }

    // Clear out because we're matching on args now, rather than a search string
    // TODO: Instead, we should keep both, and sort more intelligently. That's a higher-level task than this file.
    if (completionsFromArgMatch.length) possibleScripts = [];

    // If there are no keys at all yet, just filter the script list
    const completionsFromEntityMatch = [];
    if (!keyToken && valueToken?.value.length && possibleScripts.length > 0) {
      if (!parsed.kvMap?.size) {
        const entitySuggestions = await getAllFieldSuggestions(valueToken.value, clusterUID, client);
        const matchedByEntity = getSuggestedArgsFromPossibleEntity(possibleScripts, entitySuggestions);

        // TODO: This is wrong if we're matching on an arg with a bare value token. Gotta split that logic out.
        possibleScripts = possibleScripts.filter(({ id }) => (
          highlightMatch(valueToken.value, id).length > 0
        ));
        for (const scriptId in matchedByEntity) {
          const cs = scripts.get(scriptId);
          const goToArg = [...Object.keys(matchedByEntity[scriptId])][0];

          completionsFromEntityMatch.push({
            heading: 'Scripts',
            key: `missing_script_entity_${scriptId}`,
            description: (
              <CompletionDescription
                title={cs.id}
                body={cs.description}
                hint={`matched on argument: ${goToArg}`}
              />
            ),
            label: (
              // eslint-disable-next-line react-memo/require-usememo
              <CompletionLabel input={`${scriptId} (${goToArg})`} icon={<ScriptIcon />} highlights={[]} />
            ),
            onSelect: () => {
              const argNames = cs.vis.variables.map(v => v.name);
              argNames.sort((a, b) => +(a === 'start_time') - +(b === 'start_time'));

              let out = `script:${quoteIfNeeded(cs.id)}`;
              let caret = 0;
              for (const arg of argNames) {
                const dflt = cs.vis.variables.find(a => a.name === arg).defaultValue ?? '';
                const val = (arg === goToArg ? valueToken.value : parsed.kvMap?.get(arg)) ?? dflt;
                out += ` ${arg}:${quoteIfNeeded(val)}`;
                if (!caret || arg === goToArg) caret = out.length;
              }

              return [out, caret];
            },
          });
        }
      }
      // NOTE: If this line is reached, we have a bare value, no script, and there ARE other keys. For now, this will
      //  just stick to the arg-matching logic above and not try to handle the bare value.
    }

    if (!possibleScripts.length && !completionsFromEntityMatch.length && !completionsFromArgMatch.length) {
      return noMatchesOutput;
    }

    const highlightedIds = possibleScripts.map(s => ({
      script: s,
      highlights: highlightMatch(parsed.kvMap?.get('script') ?? valueToken?.value ?? parsed.input, s.id),
    }));

    const sort = getStringHighlightSortFn(parsed.kvMap?.get('script') ?? '');
    highlightedIds.sort((a, b) => sort(a.script.id, a.highlights, b.script.id, b.highlights));

    // TODO: The sorting and filtering here is not very good. Scratch pad should always be an option at the top, etc.
    //  Shorter matches with similar distance should come before others (`pod` should see `px/pod` before `px/node`)
    //  And we're naively finding the first match of each character, but there can be better highlights.
    //  We could do something silly: split on the slash if the partial doesn't have a slash, that'd fix that fast.

    const completionsFromNameMatch = highlightedIds.map(({ script: cs, highlights }) => ({
      heading: 'Scripts',
      key: `missing_script_${cs.id}`,
      description: <CompletionDescription title={cs.id} body={cs.description} />,
      label: (
        // eslint-disable-next-line react-memo/require-usememo
        <CompletionLabel input={cs.id} icon={<ScriptIcon />} highlights={highlights} />
      ),
      onSelect: () => {
        const argNames = cs.vis.variables.map(v => v.name);
        argNames.sort((a, b) => +(a === 'start_time') - +(b === 'start_time'));

        let out = `script:${quoteIfNeeded(cs.id)}`;
        let caret = 0;
        for (const arg of argNames) {
          const dflt = cs.vis.variables.find(a => a.name === arg).defaultValue ?? '';
          const val = parsed.kvMap?.get(arg) ?? dflt;
          out += ` ${arg}:${quoteIfNeeded(val)}`;
          if (!caret || arg === keyToken?.value) caret = out.length;
        }

        return [out, caret];
      },
    }));

    const completions = [...completionsFromNameMatch, ...completionsFromArgMatch, ...completionsFromEntityMatch];

    return {
      providerName: 'PxL Scripts',
      completions: completions.slice(0, 5),
      hasAdditionalMatches: completions.length > 5,
    };
  }, [scripts, client, clusterUID]);
};
