import './content.scss';

import {vizierGQLClient} from 'common/vizier-gql-client';
import {LineChart} from 'components/chart/line-chart';
import {ScatterPlot} from 'components/chart/scatter';
import {CodeEditor} from 'components/code-editor';
import {EXECUTE_QUERY, ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Button, Nav, Tab} from 'react-bootstrap';
import Split from 'react-split';
import {AutoSizer} from 'react-virtualized';

import {useMutation} from '@apollo/react-hooks';

import {QueryResultViewer} from '../vizier/query-result-viewer';
import {EditorTabInfo} from './editor';

const PIXIE_EDITOR_CODE_KEY_PREFIX = 'px-';
const DEFAULT_CODE = '# Enter Query Here\n';

export const EditorContent: React.FC<EditorTabInfo> = (props) => {
  const initialCode = getCodeFromStorage(props.id) || DEFAULT_CODE;
  const [code, setCode] = React.useState<string>(initialCode);

  const [runQuery, { data }] = useMutation<ExecuteQueryResult>(EXECUTE_QUERY, { client: vizierGQLClient });

  const executeQuery = (query: string) => {
    runQuery({
      variables: {
        queryStr: code,
      },
    });
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div style={{ height: '48px', display: 'flex', alignItems: 'center' }}>
        <div>{props.title}</div>
        <div className='spacer'></div>
        <Button
          size='sm'
          variant='light'
          onClick={() => executeQuery(code)}>
          Execute
        </Button>
      </div>
      <Split
        style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}
        sizes={[50, 50]}
        direction='vertical' >
        <CodeEditor
          code={code}
          onChange={(c) => {
            setCode(c);
            saveCodeToStorage(props.id, c);
          }}
        />
        <div className='pixie-editor--result-viewer'>
          {!!data ? (
            <Tab.Container defaultActiveKey='table' id='query-results-tabs'>
              <Nav variant='pills'>
                <Nav.Item>
                  <Nav.Link eventKey='table'>RESULTS</Nav.Link>
                </Nav.Item>
                <Nav.Item>
                  <Nav.Link eventKey='plot'>PLOT</Nav.Link>
                </Nav.Item>
                <Nav.Item>
                  <Nav.Link eventKey='chart'>CHART</Nav.Link>
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
            </Tab.Container>)
            : <div>No results</div>
          }
        </div>
      </Split >
    </div>
  );
};

function codeIdToKey(id: string): string {
  return `${PIXIE_EDITOR_CODE_KEY_PREFIX}${id}`;
}

function saveCodeToStorage(id: string, code: string) {
  localStorage.setItem(codeIdToKey(id), code);
}

function getCodeFromStorage(id: string): string {
  return localStorage.getItem(codeIdToKey(id)) || '';
}
