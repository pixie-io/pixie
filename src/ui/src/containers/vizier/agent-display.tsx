import './vizier.scss';

import {vizierGQLClient} from 'common/vizier-gql-client';
import {ContentBox} from 'components/content-box/content-box';
import {AutoSizedScrollableTable} from 'components/table/scrollable-table';
import {VersionInfo} from 'components/version-info/version-info';
import {distanceInWords, subSeconds} from 'date-fns';
import gql from 'graphql-tag';
import * as React from 'react';
import {Query} from 'react-apollo';
import {isProd} from 'utils/env';
import {pluralize} from 'utils/pluralize';

import {useQuery} from '@apollo/react-hooks';

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
  return `${agentCount} ${pluralize('agent', agentCount)} available`;
};

export const AgentDisplay = () => {
  const [agents, setAgents] = React.useState([]);
  const { error } = useQuery(GET_AGENTS, {
    client: vizierGQLClient,
    pollInterval: 2500,
    onCompleted: (data) => {
      setAgents(data && data.vizier && data.vizier.agents ? data.vizier.agents : []);
    },
  });
  if (error) {
    return <span>Error! {error.message}</span>;
  }
  return <AgentDisplayContent agents={agents} />;
};

const AgentDisplayContent = ({ agents }) => {
  const now = new Date();
  const mappedData = agents.map((agent) => {
    return {
      id: agent.info.id,
      hostname: agent.info.hostInfo.hostname,
      heartbeat: (agent.lastHeartbeatMs / 1000.0).toFixed(2),
      uptime: distanceInWords(subSeconds(now, agent.uptimeS), now, { addSuffix: false }),
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
      {isProd() ? <VersionInfo /> : null}
    </ContentBox>
  );
};
