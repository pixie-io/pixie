// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';

import { OperationVariables } from 'apollo-client';
import {ContentBox} from 'components/content-box/content-box';
import gql from 'graphql-tag';
import * as React from 'react';
import {Mutation, MutationFn, Query} from 'react-apollo';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import * as toml from 'toml';
import * as ResultDataUtils from 'utils/result-data-utils';
// @ts-ignore : TS does not seem to like this import.
import * as PresetQueries from './preset-queries.toml';
import {QueryResultViewer} from './query-result-viewer';

// TODO(zasgar/michelle): Figure out how to impor schema properly
import {GQLQueryResult} from '../../../../services/api/controller/schema/schema';

interface QueryManagerState {
  code: string;
  codeMirror: CodeMirror; // Ref to codeMirror component.
}

interface ExecuteQueryResult {
  ExecuteQuery: GQLQueryResult;
}

interface ResultDisplayProps {
  data: ExecuteQueryResult;
  error: string;
}

interface QueryInfoProps {
  data: ExecuteQueryResult;
}

export const GET_AGENT_IDS = gql`
{
  vizier {
    agents {
      info {
        id
      }
    }
  }
}`;

export const EXECUTE_QUERY = gql`
mutation ExecuteQuery($queryStr: String!) {
    ExecuteQuery(queryStr: $queryStr) {
      id
      table {
        relation {
          colNames
          colTypes
        }
        data
      }
    }
}
`;

const PRESET_QUERIES = toml.parse(PresetQueries).queries;

// This component displays the number of agents available to query.
const AgentCountDisplay = () => (
  <Query query={GET_AGENT_IDS} pollInterval={1000}>
  {({ loading, error, data }) => {
    if (loading) { return 'Loading...'; }
    if (error) { return `Error! ${error.message}`; }

    const agentCount = data.vizier.agents.length;
    let s = `${agentCount} agents available`;
    if (agentCount === 1) {
      s = `1 agent available`;
    } else if (agentCount === 0) {
      s = `0 agents available`;
    }

    return (
        <div>
          {s}
        </div>
    );
  }}
  </Query>
);

const ResultDisplay = (props: ResultDisplayProps) => {
  if (props.data) {
    return <QueryResultViewer data={props.data.ExecuteQuery}></QueryResultViewer>;
  }
  let body = '(No data)';
  if (props.error) {
    body = props.error;
  }
  return <div className='query-results--empty'>{body}</div>;
};

const QueryInfo = (props: QueryInfoProps) => {
  return (
    <span>
      {'Query ID: ' + props.data.ExecuteQuery.id}
      {localStorage.getItem('debug') === 'true' ?
        <span>
          {' | '}
          <a
            href='#'
            onClick={
              () => {
                const download = document.createElement('a');
                download.setAttribute('type', 'hidden');
                const blob = new Blob([ResultDataUtils.ResultsToCsv(props.data.ExecuteQuery.table.data)],
                  {type: 'octet/stream'});
                const url = window.URL.createObjectURL(blob);
                download.setAttribute('href', url);
                download.setAttribute('download', String(props.data.ExecuteQuery.id) + '.csv');
                download.click();
              }
            }
          >
            Download
          </a>
        </span>
        : null
      }

    </span>
  );
};

export class QueryManager extends React.Component<{}, QueryManagerState> {
  constructor(props) {
    super(props);

    this.state = {
      code: '# Enter Query Here\n',
      codeMirror: React.createRef(),
    };
  }

  updateCode(newCode: string) {
    this.setState({
      code: newCode,
    });
  }

  render() {
    const options = {
      lineNumbers: true,
      mode: 'python',
      theme: 'monokai',
    };

    const getCurrentQuery = () => {
      return this.state.code;
    };

    const executeQueryClickHandler = (mutationFn: MutationFn<any, OperationVariables>) => {
      mutationFn({variables: {
        queryStr: this.state.code,
      }}).catch((err) => err);
    };

    const setQuery = (eventKey, event) => {
      const key = parseInt(eventKey, 10);
      const query = PRESET_QUERIES[key][1];
      this.setState({
        code: query,
      });
      if (this.state.codeMirror.current) {
        this.state.codeMirror.current.codeMirror.setValue(query);
      }
    };
    return (
      <Mutation mutation={EXECUTE_QUERY}>
        {(executeQuery, { loading, error, data }) => (
          <div className='query-executor'>
            <div>
              <ContentBox
                headerText='Enter Query'
                secondaryText={<AgentCountDisplay/>}
              >
              <DropdownButton
                id='query-dropdown'
                title='Select a query template to start with'
              >
                {
                  PRESET_QUERIES.map((query, idx) => {
                    return <Dropdown.Item
                      key={idx}
                      eventKey={idx}
                      onSelect={setQuery.bind(this)}
                    >
                      {query[0]}
                    </Dropdown.Item>;
                  })
                }
              </DropdownButton>
              <div className='code-editor'>
                <CodeMirror
                  value={this.state.code}
                  onChange={this.updateCode.bind(this)}
                  options={options}
                  ref={this.state.codeMirror}
                />
              </div>
              <div className='query-executor--footer'>
                <div className='spacer'/>
                <Button id='execute-button' variant='primary' onClick={() => executeQueryClickHandler(executeQuery)}>
                  Execute
                </Button>
              </div>
              </ContentBox>
            </div>
            <ContentBox
              headerText={'Results'}
              subheaderText={data ? <QueryInfo data={data}/> : ''}
            >
              <ResultDisplay
                data={data}
                error={error ? error.toString() : ''}
              />
            </ContentBox>
          </div>
      )}
    </Mutation>
    );
  }
}
