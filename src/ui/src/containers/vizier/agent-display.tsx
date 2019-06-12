import {ContentBox} from 'components/content-box/content-box';
import { AutoSizedScrollableTable } from 'components/table/scrollable-table';
import {distanceInWords, subSeconds} from 'date-fns';
import gql from 'graphql-tag';
import * as React from 'react';
import { Query } from 'react-apollo';
import './vizier.scss';

export const GET_AGENTS = gql`
{
  vizier {
    agents {
      info {
        id
        hostInfo {
          hostname
        }
      }
      lastHeartbeatMs
      uptimeS
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
  label: 'Last Heartbeat (s)',
  flexGrow: 1,
  width: 30,
  resizable: true,
}, {
  dataKey: 'uptime',
  label: 'Uptime',
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

const agentString = (agentCount: number) => {
  let s = `${agentCount} agents available`;
  if (agentCount === 1) {
    s = `1 agent available`;
  } else if (agentCount === 0) {
    s = `0 agents available`;
  }
  return s;
};

export const AgentDisplay = ({onAgents}) => (
    <Query query={GET_AGENTS} pollInterval={150}>
    {({ loading, error, data }) => {
      if (loading) { return 'Loading...'; }
      if (error) { return `Error! ${error.message}`; }
      const agents = (data.vizier.agents);
      const now = new Date();
      const mappedData = agents.map((agent) => {
        return {
          id: agent.info.id,
          hostname: agent.info.hostInfo.hostname,
          heartbeat: (agent.lastHeartbeatMs / 1000.0).toFixed(2),
          uptime: distanceInWords(subSeconds(now, agent.uptimeS), now, {addSuffix: false}),
          state: agent.state,
        };
      });

      return (
        <ContentBox
          headerText='Available Agents'
          secondaryText={agentString(agents.length)}
        >
          <div className='agent-display-table'>
            <AutoSizedScrollableTable data={mappedData} columnInfo={agentTableCols}></AutoSizedScrollableTable>
          </div>
        </ContentBox>
      );
    }}
  </Query>
);
