import gql from 'graphql-tag';
import * as React from 'react';
import { Query } from 'react-apollo';
import './vizier.scss';

import { AutoSizedScrollableTable } from 'components/table/scrollable-table';

const GET_AGENTS = gql`
{
  vizier {
    agents {
      info {
        id
        hostInfo {
          hostname
        }
      }
      lastHeartbeatNs {
        l
        h
      }
      state
    }
  }
}`;

const agentTableCols = [{
  dataKey: 'id',
  label: 'Agent ID',
  flexGrow: 4,
  width: 40,
  resizable: true,
}, {
  dataKey: 'hostname',
  label: 'Hostname',
  flexGrow: 2,
  width: 40,
  resizable: true,
}, {
  dataKey: 'heartbeat',
  label: 'Heartbeat',
  flexGrow: 1,
  width: 30,
  resizable: true,
}, {
  dataKey: 'state',
  label: 'Agent State',
  flexGrow: 2,
  width: 40,
  resizable: true,
}];

export const AgentDisplay = ({onAgents}) => (
    <Query query={GET_AGENTS} pollInterval={150}>
    {({ loading, error, data }) => {
      if (loading) { return 'Loading...'; }
      if (error) { return `Error! ${error.message}`; }
      const agents = (data.vizier.agents);
      const mappedData = agents.map((agent) => {
        // TODO(zasgar): We should change this to use large ints. Or better yet,
        // in this case we should just name the times relative since clock skews
        // can cause negative numbers. After these are relative, shipping as miroseconds
        // in a 32-bit number should be ok.
        const timeInMilliAsFloat = (agent.lastHeartbeatNs.h * (2.0 ** 32) + agent.lastHeartbeatNs.l) / 1e6;
        return {
          id: agent.info.id,
          hostname: agent.info.hostInfo.hostname,
          heartbeat: (new Date().getTime() - new Date(timeInMilliAsFloat).getTime()) / 1000,
          state: agent.state,
        };
      });

      return (
          <div className='agent-display-table'>
            <AutoSizedScrollableTable data={mappedData} columnInfo={agentTableCols}/>
          </div>
      );
    }}
  </Query>
);
