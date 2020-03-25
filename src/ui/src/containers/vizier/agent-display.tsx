import './vizier.scss';

import ClientContext from 'common/vizier-grpc-client-context';
import {ContentBox} from 'components/content-box/content-box';
import {AutoSizedScrollableTable} from 'components/table/scrollable-table';
import {VersionInfo} from 'components/version-info/version-info';
import {distanceInWords} from 'date-fns';
import * as React from 'react';
import {isProd} from 'utils/env';
import {pluralize} from 'utils/pluralize';
import {dataFromProto} from 'utils/result-data-utils';
import {nanoToSeconds} from 'utils/time';

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

const POLL_INTERVAL = 2500;

interface AgentDisplayState {
  error?: string;
  data: Array<{}>;
}

const AGENT_STATUS_SCRIPT = 'px.display(px.GetAgentStatus())';

export const AgentDisplay = () => {
  const client = React.useContext(ClientContext);
  const [state, setState] = React.useState<AgentDisplayState>({ data: [] });

  React.useEffect(() => {
    if (!client) {
      return;
    }
    let mounted = true;
    const fetchAgentStatus = () => {
      client.executeScript(AGENT_STATUS_SCRIPT).then((results) => {
        if (!mounted) {
          return;
        }
        if (results.tables.length !== 1) {
          if (results.status) {
            setState({ ...state, error: results.status.getMessage() });
          }
          return;
        }
        const data = dataFromProto(results.tables[0].relation, results.tables[0].data);
        setState({ data });
      }).catch((error) => {
        if (!mounted) {
          return;
        }
        setState({ ...state, error });
      });
    };
    fetchAgentStatus();
    const interval = setInterval(fetchAgentStatus, POLL_INTERVAL);
    return () => {
      clearInterval(interval);
      mounted = false;
    };
  }, [client]);

  if (state.error) {
    return <span>Error! {state.error}</span>;
  }
  return <AgentDisplayContent agents={state.data} />;
};

export const AgentDisplayContent = ({ agents }) => {
  const now = new Date(Date.now());
  const mappedData = agents.map((agent) => {
    return {
      id: agent.agent_id,
      hostname: agent.hostname,
      heartbeat: nanoToSeconds(agent.last_heartbeat_ns).toFixed(2),
      uptime: distanceInWords(new Date(agent.create_time), now, { addSuffix: false }),
      state: agent.agent_state,
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
