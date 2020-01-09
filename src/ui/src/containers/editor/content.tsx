import './content.scss';

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
import {AutoSizer} from 'react-virtualized';

import {useMutation, useQuery} from '@apollo/react-hooks';

import {QueryResultViewer} from '../vizier/query-result-viewer';
import {getCodeFromStorage, saveCodeToStorage} from './code-utils';
import {EditorTabInfo} from './editor';

const DEFAULT_CODE = '# Enter Query Here\n';

export const EditorContent: React.FC<EditorTabInfo> = (props) => {
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
  }, []);

  const onCodeChange = React.useCallback((c) => {
    setCode(c);
    saveCodeToStorage(props.id, c);
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
          code={code}
          onChange={onCodeChange}
          onSubmit={executeQuery}
          disabled={loading}
        />
        <div className={`pixie-editor--result-viewer ${loading || error || !data ? 'center-content' : ''}`}>
          {
            loading ? <Spinner /> :
              error ? <span>{error}</span> :
                !!data ? (
                  <Tab.Container defaultActiveKey='table' id='query-results-tabs'>
                    <Nav variant='tabs' className='pixie-editor--tabs'>
                      <Nav.Item as={Nav.Link} eventKey='table'>
                        RESULTS
                      </Nav.Item>
                      <Nav.Item as={Nav.Link} eventKey='plot'>
                        PLOT
                      </Nav.Item>
                      <Nav.Item as={Nav.Link} eventKey='chart'>
                        CHART
                      </Nav.Item>
                    </Nav>
                    <Tab.Content>
                      <Tab.Pane eventKey='table'>
                        <QueryResultViewer data={data.ExecuteQuery} />
                      </Tab.Pane>
                      <Tab.Pane eventKey='plot' className='pixie-editor--tab-pane-chart'>
                        <AutoSizer>
                          {({ height, width }) => (
                            <ScatterPlot
                              data={data.ExecuteQuery}
                              height={height}
                              width={width}
                            />
                          )}
                        </AutoSizer>
                      </Tab.Pane>
                      <Tab.Pane eventKey='chart' className='pixie-editor--tab-pane-chart'>
                        <AutoSizer>
                          {({ height, width }) => (
                            <LineChart
                              data={data.ExecuteQuery}
                              height={height}
                              width={width}
                            />
                          )}
                        </AutoSizer>
                      </Tab.Pane>
                    </Tab.Content>
                  </Tab.Container>) :
                  <span>No results</span>
          }
        </div>
      </Split >
    </div>
  );
};
