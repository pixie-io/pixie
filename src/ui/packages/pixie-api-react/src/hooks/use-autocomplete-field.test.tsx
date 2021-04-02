import * as React from 'react';
import { AUTOCOMPLETE_QUERIES, GQLAutocompleteEntityKind, GQLAutocompleteSuggestion } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { MockedProvider } from '@apollo/client/testing';
import { render } from '@testing-library/react';
import { wait } from '../testing/utils';
import { useAutocompleteFieldSuggester } from './use-autocomplete-field';

describe('useAutocompleteFieldSuggester to suggest available entities from user input', () => {
  const good = [{
    request: {
      query: AUTOCOMPLETE_QUERIES.FIELD,
      variables: {
        clusterUID: 'fooCluster',
        input: 'scr',
        kind: 'AEK_SCRIPT',
      },
    },
    result: {
      data: {
        autocompleteField: [
          { kind: 'AEK_SCRIPT', name: 'scriptId', description: 'scriptId does the thing' },
          { kind: 'AEK_SCRIPT', name: 'scripted', description: 'does some other things.' },
        ],
      },
    },
  }];

  const bad = [{
    request: good[0].request,
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  /*
   * Not using itPassesBasicHookTests, because this hook behaves much differently. Unlike most of the direct
   * Apollo query hooks, this one provides a callback that in turn executes (memoized) queries and transforms
   * those into Promise objects.
   */

  const Consumer: React.FC = () => {
    const [result, setResult] = React.useState<GQLAutocompleteSuggestion[]>(null);
    const [error, setError] = React.useState<ApolloError>(null);
    const getCompletions = useAutocompleteFieldSuggester('fooCluster');
    React.useMemo(() => {
      getCompletions('scr', GQLAutocompleteEntityKind.AEK_SCRIPT).then(setResult).catch(setError);
    }, [getCompletions]);

    return <>{ JSON.stringify(result ?? error) }</>;
  };

  const getConsumer = (mocks) => render(
    <MockedProvider mocks={mocks} addTypename={false}>
      <Consumer />
    </MockedProvider>,
  );

  it('Provides a promise of eventual results', async () => {
    const consumer = getConsumer(good);
    expect(consumer.baseElement.textContent).toEqual('null');
    await wait();
    expect(consumer.baseElement.textContent)
      .toEqual(JSON.stringify(good[0].result.data.autocompleteField));
  });

  it('Rejects the promise if something goes wrong', async () => {
    const consumer = getConsumer(bad);
    await wait();
    expect(consumer.baseElement.textContent).toEqual(JSON.stringify(bad[0].error));
  });
});
