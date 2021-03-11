import * as React from 'react';
import {
  AUTOCOMPLETE_QUERIES,
  GQLAutocompleteActionType, GQLAutocompleteResult,
} from '@pixie/api';
import { useApolloClient } from '@apollo/client';

export type AutocompleteSuggester = (
  input: string, cursor: number, action: GQLAutocompleteActionType
) => Promise<GQLAutocompleteResult>;

export function useAutocomplete(clusterUID: string): AutocompleteSuggester {
  const client = useApolloClient();
  return React.useCallback((input: string, cursor: number, action: GQLAutocompleteActionType) => (
    client.query<{ autocomplete: GQLAutocompleteResult }>({
      query: AUTOCOMPLETE_QUERIES.AUTOCOMPLETE,
      fetchPolicy: 'network-only',
      variables: {
        input,
        cursor,
        action,
        clusterUID,
      },
    }).then(
      (results) => results.data.autocomplete,
    )), [client, clusterUID]);
}
