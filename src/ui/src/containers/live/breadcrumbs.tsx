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
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { isPixieEmbedded } from 'app/common/embed-context';
import {
  Breadcrumbs, BreadcrumbOptions, StatusCell,
} from 'app/components';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { SCRATCH_SCRIPT, ScriptsContext } from 'app/containers/App/scripts-context';
import { pxTypeToEntityType, entityStatusGroup } from 'app/containers/live/autocomplete-utils';
import { ScriptContext } from 'app/context/script-context';
import { GQLAutocompleteEntityKind, GQLAutocompleteFieldResult } from 'app/types/schema';
import { argVariableMap, argTypesForVis } from 'app/utils/args-utils';
import { highlightNamespacedScoredMatch, highlightScoredMatch } from 'app/utils/string-search';
import { BreadcrumbExtras } from 'configurable/breadcrumb-extras';
import { TimeArgDetail } from 'configurable/time-arg-detail';

import { Variable } from './vis';

type AutocompleteFieldSuggester = (
  input: string, kind: GQLAutocompleteEntityKind
) => Promise<GQLAutocompleteFieldResult>;

/**
 * Given a cluster to check against, returns an @link{AutocompleteFieldSuggester} for that cluster.
 *
 * Usage:
 * ```
 * const getCompletions = useAutocompleteSuggestion('fooCluster');
 * React.useEffect(() => {
 *   // Might suggest the script `px/http_data` as a completion for `px/htt`
 *   getCompletions('htt', 'AEK_SCRIPT').then(suggestions => suggestions.forEach(s => doSomethingMeaningful(s)));
 * }, [getCompletions]);
 * ```
 */
function useAutocompleteFieldSuggester(clusterUID: string): AutocompleteFieldSuggester {
  const client = React.useContext(PixieAPIContext) as PixieAPIClient;
  return React.useCallback<AutocompleteFieldSuggester>((partialInput: string, kind: GQLAutocompleteEntityKind) => (
    client.getCloudClient().graphQL.query<{ autocompleteField: GQLAutocompleteFieldResult }>({
      query: gql`
        query getCompletions($input: String, $kind: AutocompleteEntityKind, $clusterUID: String) {
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
        input: partialInput,
        kind,
        clusterUID,
      },
    }).then(
      (results) => results.data.autocompleteField,
    )), [client, clusterUID]);
}

const useStyles = makeStyles(({ spacing }: Theme) => createStyles({
  spacer: {
    flex: 1,
  },
  breadcrumbs: {
    display: 'flex',
    // Use an over sized element to let shadows always appear,
    // but let a horizontal scrollbar show up if there are more breadcrumbs than space allows.
    margin: `${spacing(-0.5)} ${spacing(2.5)}`,
    marginRight: spacing(2),
    padding: spacing(0.5),
    paddingBottom: spacing(1),
    overflow: 'auto hidden',
    position: 'relative',
  },
  extras: {
    textAlign: 'right',
    marginRight: spacing(2.5),
    marginTop: spacing(0.5),
    marginBottom: spacing(-2),
    zIndex: 1, // So it's clickable above the padding of the widgets
  },
}), { name: 'LiveViewBreadcrumbs' });

export const LiveViewBreadcrumbs: React.FC = React.memo(() => {
  const classes = useStyles();
  const { selectedClusterUID, selectedClusterName } = React.useContext(ClusterContext);
  const { scripts } = React.useContext(ScriptsContext);

  const {
    args, script, setScriptAndArgs,
  } = React.useContext(ScriptContext);

  const { embedState: { disableTimePicker, widget } } = React.useContext(LiveRouteContext);
  const isEmbedded = isPixieEmbedded();

  const getCompletions = useAutocompleteFieldSuggester(selectedClusterUID);

  const scriptIds = React.useMemo(() => [...scripts.keys()], [scripts]);

  // For useMemo dependencies below, see https://github.com/facebook/react/issues/20204
  const collapsedScriptIds = scriptIds?.join(',');

  type MemoCrumbs = { entityBreadcrumbs: BreadcrumbOptions[]; argBreadcrumbs: BreadcrumbOptions[] };
  const { entityBreadcrumbs, argBreadcrumbs }: MemoCrumbs = React.useMemo(() => {
    // eslint-disable-next-line @typescript-eslint/no-shadow
    const entityBreadcrumbs: BreadcrumbOptions[] = [];
    // eslint-disable-next-line @typescript-eslint/no-shadow
    const argBreadcrumbs: BreadcrumbOptions[] = [];

    // Add script at beginning of breadcrumbs.
    entityBreadcrumbs.push({
      title: 'script',
      value: script?.id,
      selectable: true,
      allowTyping: true,
      divider: true,
      getListItems: async (input) => {
        const matches = new Map(input ? scriptIds.map(s => [s, highlightNamespacedScoredMatch(input, s, '/')]) : []);

        const ids = scriptIds.filter(s => !input || s === SCRATCH_SCRIPT.id || matches.get(s)?.isMatch);

        // The `px` namespace should appear before all others
        ids.sort((a, b) => Number(b.startsWith('px/')) - Number(a.startsWith('px/')));

        // Higher quality matches appear before lower ones (with the `px/` namespace winning on a tie)
        ids.sort((a, b) => (matches.get(a)?.distance ?? Infinity) - (matches.get(b)?.distance ?? Infinity));

        // The scratch script should always appear at the top of the list for visibility. It doesn't get auto-selected
        // unless it's the only thing in the list.
        const scratchIndex = ids.indexOf(SCRATCH_SCRIPT.id);
        if (scratchIndex !== -1) {
          ids.splice(scratchIndex, 1);
          // Don't include SCRATCH in embedded views.
          if (!isEmbedded) {
            ids.unshift(SCRATCH_SCRIPT.id);
          }
        }

        const items = ids.map((scriptId) => ({
          value: scriptId,
          description: scripts.get(scriptId).description,
          autoSelectPriority: scriptId === SCRATCH_SCRIPT.id ? -1 : 0,
          highlights: scriptId === SCRATCH_SCRIPT.id ? [] : (matches.get(scriptId)?.highlights ?? []),
        }));
        return { items, hasMoreItems: false };
      },
      onSelect: (newVal) => {
        const newScript = scripts.get(newVal);
        setScriptAndArgs(newScript, args);
      },
    });

    // Add args to breadcrumbs.
    const argTypes = argTypesForVis(script?.vis);
    const variables = argVariableMap(script?.vis);

    for (const [argName, argVal] of Object.entries(args)) {
      // Only add suggestions if validValues are specified. Otherwise, the dropdown is populated with autocomplete
      // entities or has no elements and the user must manually type in values.
      const variable: Variable = variables[argName];

      const argProps: BreadcrumbOptions = {
        title: argName,
        value: argVal?.toString(),
        selectable: true,
        allowTyping: true,
        onSelect: (newVal: string) => {
          const val = newVal?.trim() || variable?.defaultValue?.trim() || '';
          setScriptAndArgs(script, { ...args, [argName]: val });
        },
        getListItems: null,
        requireCompletion: false,
        placeholder: variable?.description,
        explanation: null,
      };

      if (variable && typeof variable.defaultValue === 'undefined') {
        argProps.title += '*';
      }

      if (variable?.validValues?.length) {
        argProps.getListItems = async (input) => ({
          items: variable.validValues
            .map(value => ({
              value,
              match: input === '' ? { isMatch: true, distance: 0, highlights: [] } : highlightScoredMatch(input, value),
            }))
            .filter(({ match }) => match.isMatch)
            .sort((a, b) => a.match.distance - b.match.distance)
            .map(({ value, match }) => ({
              value,
              description: '',
              highlights: match.highlights,
            })),
          hasMoreItems: false,
        });

        argProps.requireCompletion = true;
      }

      const entityType = pxTypeToEntityType(argTypes[argName]);
      if (entityType !== 'AEK_UNKNOWN') {
        argProps.getListItems = async (input) => {
          const { suggestions, hasAdditionalMatches } = await getCompletions(input, entityType);
          const items = suggestions.map((suggestion) => ({
            value: suggestion.name,
            description: suggestion.description,
            highlights: suggestion.matchedIndexes,
            icon: <StatusCell statusGroup={entityStatusGroup(suggestion.state)} />,
          }));
          return { items, hasMoreItems: hasAdditionalMatches };
        };
      }

      // TODO(michelle): Ideally we should just be able to use the entityType to determine whether the
      // arg appears in the argBreadcrumbs. However, some entities still don't have a corresponding entity type
      // (such as nodes), since they are not yet supported in autocomplete. Until that is fixed, this is hard-coded
      // for now.
      if (argName === 'start_time' || argName === 'start') {
        // Don't show the time picker at all if it is disabled.
        if (!disableTimePicker) {
          argProps.explanation = <TimeArgDetail clusterName={selectedClusterName} />;
          argBreadcrumbs.push(argProps);
        }
      } else {
        entityBreadcrumbs.push(argProps);
      }
    }

    return { entityBreadcrumbs, argBreadcrumbs };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [script?.id, script?.code, script?.vis, collapsedScriptIds, args, selectedClusterUID, getCompletions]);

  if (widget) {
    return <></>;
  }

  const extras = <BreadcrumbExtras />;

  return (
    <>
      <div className={classes.breadcrumbs}>
        <Breadcrumbs
          breadcrumbs={entityBreadcrumbs}
        />
        <div className={classes.spacer} />
        <Breadcrumbs
          breadcrumbs={argBreadcrumbs}
        />
      </div>
      { extras && <div className={classes.extras}><BreadcrumbExtras /></div> }
    </>
  );
});
LiveViewBreadcrumbs.displayName = 'LiveViewBreadcrumbs';
