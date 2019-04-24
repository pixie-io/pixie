// Import code mirror dependencies.
import 'codemirror/lib/codemirror.css';
import 'codemirror/mode/python/python';
import 'codemirror/theme/monokai.css';

import { OperationVariables } from 'apollo-client';
import gql from 'graphql-tag';
import * as React from 'react';
import {Mutation, MutationFn, Query} from 'react-apollo';
import {Button} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import {QueryResultViewer} from './query-result-viewer';

// TODO(zasgar/michelle): Figure out how to impor schema properly
import {GQLQueryResult} from '../../../../services/api/controller/schema/schema';

interface QueryManagerState {
  code: string;
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

    return (<div className='query-executor'>
      <AgentCountDisplay/>
      <span>Query:</span>
      <div className='code-editor'>
        <CodeMirror value={this.state.code} onChange={this.updateCode.bind(this)} options={options} />
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
