// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';

import { OperationVariables } from 'apollo-client';
import gql from 'graphql-tag';
import * as React from 'react';
import {Mutation, MutationFn, Query} from 'react-apollo';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import * as toml from 'toml';
// @ts-ignore : TS does not seem to like this import.
import * as PresetQueries from './preset-queries.toml';
import {QueryResultViewer} from './query-result-viewer';

// TODO(zasgar/michelle): Figure out how to impor schema properly
import {GQLQueryResult} from '../../../../services/api/controller/schema/schema';

interface QueryManagerState {
  code: string;
  codeMirror: CodeMirror; // Ref to codeMirror component.
}

const GET_AGENT_IDS = gql`
{
  vizier {
    agents {
      info {
        id
      }
    }
  }
}`;

const EXECUTE_QUERY = gql`
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
      let s = `There are ${agentCount} agents available.`;
      if (agentCount === 1) {
        s = `There is 1 agent available.`;
      } else if (agentCount === 0) {
        s = `There are no agents available.`;
      }

      return (
          <div>
            {s}
          </div>
      );
    }}
    </Query>
);

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
      }});
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
    return (<div className='query-executor'>
      <AgentCountDisplay/>
      <DropdownButton
        id='query-dropdown'
        title='Select a query template to start with'
      >
        {
          PRESET_QUERIES.map((query, idx) => {
            return <Dropdown.Item key={idx} eventKey={idx} onSelect={setQuery.bind(this)}>{query[0]}</Dropdown.Item>;
          })
        }
      </DropdownButton>
      <span>Query:</span>
      <div className='code-editor'>
        <CodeMirror
          value={this.state.code}
          onChange={this.updateCode.bind(this)}
          options={options}
          ref={this.state.codeMirror}
        />
      </div>
      <Mutation mutation={EXECUTE_QUERY}>
        {(executeQuery, { data }) => (
            <div>
              <Button variant='primary' onClick={() => executeQueryClickHandler(executeQuery)}>
                Execute Query
              </Button>
              {data && <QueryResultViewer data={data.ExecuteQuery}></QueryResultViewer>}
            </div>
        )}
      </Mutation>
      <br/>
    </div>);
  }
}
