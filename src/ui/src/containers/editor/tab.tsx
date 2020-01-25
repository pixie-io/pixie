import './tab.scss';

import {APPEND_HISTORY} from 'common/local-gql';
import {vizierGQLClient} from 'common/vizier-gql-client';
import {LineChart} from 'components/chart/line-chart';
import {ScatterPlot} from 'components/chart/scatter';
import {CodeEditor} from 'components/code-editor';
import {Spinner} from 'components/spinner/spinner';
import {EXECUTE_QUERY, ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Button, Nav, Tab} from 'react-bootstrap';
import Split from 'react-split';

import {useMutation, useQuery} from '@apollo/react-hooks';

import {getCodeFromStorage, saveCodeToStorage} from './code-utils';
import {EditorTabInfo} from './editor';
import {ConsoleResults} from './results';

const DEFAULT_CODE = '# Enter Query Here\n';

export const ConsoleTab: React.FC<EditorTabInfo> = (props) => {
  const initialCode = getCodeFromStorage(props.id) || DEFAULT_CODE;
  const [code, setCode] = React.useState<string>(initialCode);
  const [error, setError] = React.useState('');

  const [runQuery, { data, loading }] = useMutation<ExecuteQueryResult>(EXECUTE_QUERY, {
    client: vizierGQLClient,
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
    }).then(({ errors }) => {
      saveHistory({
        variables: {
          history: {
            time,
            code,
            title: props.title,
            status: !errors ? 'FAILED' : 'SUCCESS',
          },
        },
      });
    });
  }, [code]);

  const updateVoyager = React.useCallback(() => {
    props.updateVoyager();
  }, []);
  // Pass up data to Editor to send to Voyager.
  if (data) {
    if (data.ExecuteQuery.table && data.ExecuteQuery.table.length > 0) {
      props.updateResults(data.ExecuteQuery.table[0].data);
    }
  }

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
        {
          localStorage.getItem('px-voyager') === 'true' ?
            <Button
              size='sm'
              variant='primary'
              onClick={updateVoyager}
              style={{ marginLeft: '5px'}}
            >
              Voyager
            </Button> : null
        }
      </div>
      <Split
        style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}
        sizes={[50, 50]}
        gutterSize={5}
        direction='vertical' >
        <CodeEditor
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
