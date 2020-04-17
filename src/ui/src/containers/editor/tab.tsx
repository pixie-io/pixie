import './tab.scss';

import {APPEND_HISTORY} from 'common/local-gql';
import {VizierQueryResult} from 'common/vizier-grpc-client';
import VizierGRPCClientContext from 'common/vizier-grpc-client-context';
import {CodeEditor} from 'components/code-editor';
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
  const [error, setError] = React.useState<Error>(null);
  const [data, setData] = React.useState<VizierQueryResult>(null);
  const [loading, setLoading] = React.useState(false);
  const client = React.useContext(VizierGRPCClientContext);

  const [saveHistory] = useMutation(APPEND_HISTORY);

  const executeQuery = React.useCallback(() => {
    if (!client) {
      return;
    }
    const time = new Date();
    let queryId;
    let errMsg: string;
    setLoading(true);
    client.executeScriptOld(code).then((results) => {
      queryId = results.queryId;
      setData(results);
      setError(null);
    }).catch((e) => {
      errMsg = e.message;
      setError(e);
    }).finally(() => {
      setLoading(false);
      analytics.track('Query Execution', {
        status: errMsg ? 'success' : 'failed',
        query: code,
        queryID: queryId,
        error: errMsg,
        title: props.title,
      });
      saveHistory({
        variables: {
          history: {
            time,
            code,
            title: props.title,
            status: errMsg ? 'success' : 'failed',
          },
        },
      });
    });
  }, [client, code]);

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
          disabled={loading || !client}
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
