import './content.scss';

import {vizierGQLClient} from 'common/vizier-gql-client';
import {CodeEditor} from 'components/code-editor';
import {EXECUTE_QUERY, ExecuteQueryResult} from 'gql-types';
import * as React from 'react';
import {Button} from 'react-bootstrap';
import Split from 'react-split';

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
          {!!data ? <QueryResultViewer data={data.ExecuteQuery} /> : <div>No results</div>}
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
