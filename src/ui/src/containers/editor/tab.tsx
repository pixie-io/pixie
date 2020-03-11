import './tab.scss';

import {APPEND_HISTORY} from 'common/local-gql';
import {VizierGQLClient, VizierGQLClientContext} from 'common/vizier-gql-client';
import {CodeEditor} from 'components/code-editor';
import {EXECUTE_QUERY, ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Button, Nav, Tab} from 'react-bootstrap';
import Split from 'react-split';
import analytics from 'utils/analytics';

import {useMutation} from '@apollo/react-hooks';

import {getCodeFromStorage, saveCodeToStorage} from './code-utils';
import {EditorTabInfo} from './editor';
import {ConsoleResults} from './results';

const DEFAULT_CODE = '# Enter Query Here\n';

export const ConsoleTab: React.FC<EditorTabInfo> = (props) => {
  const initialCode = getCodeFromStorage(props.id) || DEFAULT_CODE;
  const [code, setCode] = React.useState<string>(initialCode);
  const [error, setError] = React.useState('');
  const vizierClient = React.useContext(VizierGQLClientContext);

  const [runQuery, { data, loading }] = useMutation<ExecuteQueryResult>(EXECUTE_QUERY, {
    client: vizierClient.gqlClient,
    onError: (e) => {
      setError('Request failed! Please try again later.');
    },
    onCompleted: () => {
      setError('');
    },
  });

  const [saveHistory] = useMutation(APPEND_HISTORY);

  const executeQuery = React.useCallback(() => {
    const time = new Date();
    runQuery({
      variables: {
        queryStr: code,
      },
    }).then((results) => {
      let err = '';
      if (!results) {
        err = 'unknown error';
      } else if (results.errors) {
        err = results.errors.join(', ');
      } else if (
        // TODO(malthus): Remove this once we switch to eslint.
        // tslint:disable-next-line:whitespace
        results.data?.ExecuteQuery?.error?.compilerError) {
        err = 'compiler error';
      }
      analytics.track('Query Execution', {
        status: !err ? 'success' : 'failed',
        query: code,
        // TODO(malthus): Remove this once we switch to eslint.
        // tslint:disable-next-line:whitespace
        queryID: results?.data?.ExecuteQuery?.id,
        error: err,
      });
      saveHistory({
        variables: {
          history: {
            time,
            code,
            title: props.title,
            status: !err ? 'SUCCESS' : 'FAILED',
          },
        },
      });
    });
  }, [code]);

  React.useEffect(() => {
    saveCodeToStorage(props.id, code);
  }, [code]);

  const onCodeChange = React.useCallback((c) => {
    setCode(c);
  }, []);

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div className='pixie-editor--content-title-row'>
        <div className='text-lighter'>{props.title}</div>
        <div className='spacer'></div>
        <Button
          size='sm'
          variant='secondary'
          disabled={loading}
          onClick={executeQuery}>
          Execute
        </Button>
      </div>
      <Split
        style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}
        sizes={[50, 50]}
        gutterSize={5}
        direction='vertical' >
        <CodeEditor
          className='pixie-code-editor'
          code={code}
          onChange={onCodeChange}
          onSubmit={executeQuery}
          disabled={loading}
        />
        <ConsoleResults error={error} loading={loading} data={data} code={code} />
      </Split >
    </div>
  );
};
