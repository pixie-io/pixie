import * as React from 'react';
import {
  AUTOCOMPLETE_QUERIES,
  GQLAutocompleteActionType,
  GQLAutocompleteEntityKind,
  GQLAutocompleteResult,
} from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { MockedProvider } from '@apollo/client/testing';
import { render } from '@testing-library/react';
import { wait } from '../testing/utils';
import { useAutocomplete } from './use-autocomplete';

describe('useAutocomplete to suggest available entities from user input', () => {
  const good = () => [
    {
      request: {
        query: AUTOCOMPLETE_QUERIES.AUTOCOMPLETE,
        variables: {
          clusterUID: 'fooCluster',
          cursor: 6,
          input: 'px/htt',
          action: GQLAutocompleteActionType.AAT_EDIT,
        },
      },
      result: {
        data: {
          autocomplete: [{
            formattedInput: 'px/htt',
            isExecutable: false,
            tabSuggestions: [{
              tabIndex: 0,
              executableAfterSelect: true,
              suggestions: [{
                kind: GQLAutocompleteEntityKind.AEK_SCRIPT,
                name: 'px/http_data',
                description: 'A long form description exists here in the real, not-lazy data',
              }],
            }],
          }],
        },
      },
    },
    {
      request: {
        query: AUTOCOMPLETE_QUERIES.AUTOCOMPLETE,
        variables: {
          clusterUID: 'fooCluster',
          cursor: 6,
          input: 'px/htt',
          action: GQLAutocompleteActionType.AAT_SELECT,
        },
      },
      result: {
        data: {
          autocomplete: [{
            formattedInput: 'px/htt',
            isExecutable: false,
            tabSuggestions: [{
              tabIndex: 0,
              executableAfterSelect: true,
              suggestions: [{
                kind: GQLAutocompleteEntityKind.AEK_SCRIPT,
                name: 'px/http_data',
                description: 'A long form description exists here in the real, not-lazy data',
              }],
            }],
          }],
        },
      },
    },
  ];

  const bad = () => [
    {
      request: good()[0].request,
      error: new ApolloError({ errorMessage: 'Request failed!' }),
    },
    {
      request: good()[1].request,
      error: new ApolloError({ errorMessage: 'Request failed!' }),
    },
  ];

  /*
   * Not using itPassesBasicHookTests, because this hook behaves much differently. Unlike most of the direct
   * Apollo query hooks, this one provides a callback that in turn executes (memoized) queries and transforms
   * those into Promise objects.
   */

  const Consumer: React.FC<{action: GQLAutocompleteActionType}> = ({ action }) => {
    const [result, setResult] = React.useState<GQLAutocompleteResult>(null);
    const [error, setError] = React.useState<ApolloError>(null);
    const runAutocomplete = useAutocomplete('fooCluster');
    React.useMemo(() => {
      runAutocomplete('px/htt', 6, action).then(setResult).catch(setError);
    }, [action, runAutocomplete]);

    return <>{ JSON.stringify(result ?? error) }</>;
  };

  const getConsumer = (mocks, action: GQLAutocompleteActionType) => render(
    <MockedProvider mocks={mocks} addTypename={false}>
      <Consumer action={action} />
    </MockedProvider>,
  );

  it('Provides a promise of eventual results (edit)', async () => {
    const editor = getConsumer(good(), GQLAutocompleteActionType.AAT_EDIT);
    await wait();
    expect(editor.baseElement.textContent)
      .toEqual(JSON.stringify(good()[0].result.data.autocomplete));
  });

  it('Provides a promise of eventual results (select)', async () => {
    const selector = getConsumer(good(), GQLAutocompleteActionType.AAT_SELECT);
    await wait();
    expect(selector.baseElement.textContent)
      .toEqual(JSON.stringify(good()[1].result.data.autocomplete));
  });

  it('Rejects the promise if something goes wrong (edit)', async () => {
    const editor = getConsumer(bad(), GQLAutocompleteActionType.AAT_EDIT);
    await wait();
    expect(editor.baseElement.textContent).toEqual(JSON.stringify(bad()[0].error));
  });

  it('Rejects the promise if something goes wrong (select)', async () => {
    const selector = getConsumer(bad(), GQLAutocompleteActionType.AAT_SELECT);
    await wait();
    expect(selector.baseElement.textContent).toEqual(JSON.stringify(bad()[1].error));
  });
});
