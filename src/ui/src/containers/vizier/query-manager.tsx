// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';

import {OperationVariables} from 'apollo-client';
import {vizierGQLClient} from 'common/vizier-gql-client';
import {ContentBox} from 'components/content-box/content-box';
import gql from 'graphql-tag';
// @ts-ignore : TS does not like image files.
import * as loadingSvg from 'images/icons/Loading.svg';
import * as React from 'react';
import {Mutation, MutationFn, Query} from 'react-apollo';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import {HotKeys} from 'react-hotkeys';
import {Link} from 'react-router-dom';
import * as toml from 'toml';
import {pluralize} from 'utils/pluralize';
import * as ResultDataUtils from 'utils/result-data-utils';

// TODO(zasgar/michelle): Figure out how to impor schema properly
import {GQLQueryResult} from '../../../../vizier/services/api/controller/schema/schema';
// @ts-ignore : TS does not seem to like this import.
import * as PresetQueries from './preset-queries.toml';
import {QueryResultViewer} from './query-result-viewer';

const HOT_KEY_MAP = {
  EXECUTE_QUERY: ['ctrl+enter', 'command+enter'],
};

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
  loading: boolean;
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
      error {
        compilerError {
          msg
          lineColErrors {
            line
            col
            msg
          }
        }
      }
    }
}
`;

const PRESET_QUERIES = toml.parse(PresetQueries).queries;

// This component displays the number of agents available to query.
const AgentCountDisplay = () => (
  <Query client={vizierGQLClient} query={GET_AGENT_IDS} pollInterval={1000}>
    {({ loading, error, data }) => {
      if (loading) { return 'Loading...'; }
      if (error) { return `Error! ${error.message}`; }

      const agentCount = data.vizier.agents.length;
      const s = `${agentCount} ${pluralize('agent', agentCount)} available`;

      return <Link to='/vizier/agents'>{s}</Link>;
    }}
  </Query>
);

const ResultDisplay = (props: ResultDisplayProps) => {
  if (props.data) {
    return <QueryResultViewer data={props.data.ExecuteQuery}></QueryResultViewer>;
  }
  let body = <div>(No data)</div>;
  if (props.error) {
    body = <div>{props.error}</div>;
  } else if (props.loading) {
    body = <img className='spinner' src={loadingSvg} />;
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
                  { type: 'octet/stream' });
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
    let code = localStorage.getItem('savedCode');
    if (!code) {
      code = '# Enter Query Here\n';
    }
    this.state = {
      code,
      codeMirror: React.createRef(),
    };
  }

  updateCode(newCode: string) {
    this.setState({
      code: newCode,
    });
    localStorage.setItem('savedCode', newCode);
  }

  renderCodeEditor(executeQueryFn) {
    const options = {
      lineNumbers: true,
      mode: 'python',
      theme: 'monokai',
      extraKeys: {
        'Cmd-Enter': executeQueryFn,
        'Ctrl-Enter': executeQueryFn,
      },
    };
    // TODO(philkuz) (PL-??) pass in the ExecuteQueryResult data to populate
    // code mirror nnotations. See the folowing for an example:
    // https://github.com/codemirror/CodeMirror/blob/master/addon/lint/lint.js#L160


    return (<CodeMirror
      value={this.state.code}
      onChange={this.updateCode.bind(this)}
      options={options}
      ref={this.state.codeMirror}
    />);
  }

  render() {
    const getCurrentQuery = () => {
      return this.state.code;
    };

    const executeQueryClickHandler = (mutationFn: MutationFn<any, OperationVariables>) => {
      mutationFn({
        variables: {
          queryStr: this.state.code,
        },
      }).catch((err) => err);
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
      <Mutation client={vizierGQLClient} mutation={EXECUTE_QUERY}>
        {(executeQuery, { loading, error, data }) => (
          <HotKeys
            className='hotkey-container'
            attach={window}
            focused={true}
            keyMap={HOT_KEY_MAP}
            handlers={{ EXECUTE_QUERY: () => executeQueryClickHandler(executeQuery) }}
          >
            <div className='query-executor'>
              <div>
                <ContentBox
                  headerText='Enter Query'
                  initialHeight={250}
                  resizable={true}
                  secondaryText={<AgentCountDisplay />}
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
                    {this.renderCodeEditor(() => executeQueryClickHandler(executeQuery))}
                  </div>
                  <div className='query-executor--footer'>
                    <div className='spacer' />
                    <Button
                      id='execute-button'
                      variant='primary'
                      onClick={() => executeQueryClickHandler(executeQuery)}
                    >
                      Execute
                    </Button>
                  </div>
                </ContentBox>
              </div>
              <ContentBox
                headerText={'Results'}
                subheaderText={data ? <QueryInfo data={data} /> : ''}
              >
                <ResultDisplay
                  data={data}
                  error={error ? error.toString() : ''}
                  loading={loading}
                />
              </ContentBox>
            </div>
          </HotKeys>
        )}
      </Mutation>
    );
  }
}
