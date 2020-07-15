import * as React from 'react';

import ClientContext, {
  VizierGRPCClientProvider, CLUSTER_STATUS_DISCONNECTED,
} from 'common/vizier-grpc-client-context';
import PixieBreadcrumbs from 'components/breadcrumbs/breadcrumbs';
import { StatusCell, StatusGroup } from 'components/status/status';
import ProfileMenu from 'containers/profile-menu/profile-menu';
import { distanceInWords } from 'date-fns';
import gql from 'graphql-tag';
import { useHistory, useParams } from 'react-router';
import { Link } from 'react-router-dom';
import { dataFromProto } from 'utils/result-data-utils';

import { useQuery } from '@apollo/react-hooks';

import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import Breadcrumbs from '@material-ui/core/Breadcrumbs';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableContainer from '@material-ui/core/TableContainer';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';

import {
  AdminTooltip, agentStatusGroup, convertHeartbeatMS, getClusterDetailsURL,
  podStatusGroup, StyledLeftTableCell, StyledRightTableCell, StyledTab, StyledTableCell,
  StyledTableHeaderCell, StyledTabs,
} from './utils';
import { formatUInt128 } from '../../utils/format-data';

const StyledBreadcrumbLink = withStyles((theme: Theme) => ({
  root: {
    ...theme.typography.body2,
    display: 'flex',
    alignItems: 'center',
    paddingLeft: theme.spacing(1),
    paddingRight: theme.spacing(1),
    height: theme.spacing(3),
    color: theme.palette.foreground.grey5,
  },
}))(({ classes, children, to }: any) => (
  <Link className={classes.root} to={to}>{children}</Link>
));

const StyledBreadcrumbs = withStyles((theme: Theme) => ({
  root: {
    display: 'flex',
    paddingTop: theme.spacing(1),
    paddingBottom: theme.spacing(1),
    marginRight: theme.spacing(4.5),
    marginLeft: theme.spacing(3),
    marginBottom: theme.spacing(1),
  },
  separator: {
    display: 'flex',
    alignItems: 'center',
    color: theme.palette.foreground.one,
    fontWeight: 1000,
    width: theme.spacing(1),
  },
}))(({ classes, children }: any) => (
  <Breadcrumbs classes={classes}>
    {children}
  </Breadcrumbs>
));

const AGENT_STATUS_SCRIPT = `import px
px.display(px.GetAgentStatus())`;

const AGENTS_POLL_INTERVAL = 2500;

interface AgentDisplay {
  id: string;
  idShort: string;
  status: string;
  statusGroup: StatusGroup;
  hostname: string;
  lastHeartbeat: string;
  uptime: string;
}

export function formatAgent(agentInfo): AgentDisplay {
  const now = new Date();
  const agentID = formatUInt128(agentInfo.agent_id);
  return {
    id: agentID,
    idShort: agentID.split('-').pop(),
    status: agentInfo.agent_state.replace('AGENT_STATE_', ''),
    statusGroup: agentStatusGroup(agentInfo.agent_state),
    hostname: agentInfo.hostname,
    lastHeartbeat: convertHeartbeatMS(agentInfo.last_heartbeat_ns / (1000 * 1000)),
    uptime: distanceInWords(new Date(agentInfo.create_time), now, { addSuffix: false }),
  };
}

const AgentsTableContent = ({ agents }) => {
  const agentsDisplay = agents.map((agent) => formatAgent(agent));
  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell />
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
          <StyledTableHeaderCell>Hostname</StyledTableHeaderCell>
          <StyledTableHeaderCell>Last Heartbeat</StyledTableHeaderCell>
          <StyledTableHeaderCell>Uptime</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {agentsDisplay.map((agent) => (
          <TableRow key={agent.id}>
            <AdminTooltip title={agent.status}>
              <StyledLeftTableCell>
                <StatusCell statusGroup={agent.statusGroup} />
              </StyledLeftTableCell>
            </AdminTooltip>
            <AdminTooltip title={agent.id}>
              <StyledTableCell>{agent.idShort}</StyledTableCell>
            </AdminTooltip>
            <StyledTableCell>{agent.hostname}</StyledTableCell>
            <StyledTableCell>{agent.lastHeartbeat}</StyledTableCell>
            <StyledRightTableCell>{agent.uptime}</StyledRightTableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
};

interface AgentDisplayState {
  error?: string;
  data: Array<{}>;
}

const AgentsTable = () => {
  const { client } = React.useContext(ClientContext);
  const [state, setState] = React.useState<AgentDisplayState>({ data: [] });

  React.useEffect(() => {
    if (!client) {
      return;
    }
    let mounted = true;
    const fetchAgentStatus = () => {
      client.executeScript(AGENT_STATUS_SCRIPT, []).then((results) => {
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
        setState({ ...state, error: error?.message });
      });
    };
    fetchAgentStatus();
    const interval = setInterval(fetchAgentStatus, AGENTS_POLL_INTERVAL);
    return () => {
      clearInterval(interval);
      mounted = false;
    };
  }, [client, state]);

  if (state.error) {
    return (
      <span>
        Error!
        {state.error}
      </span>
    );
  }
  return <AgentsTableContent agents={state.data} />;
};

// TODO(nserrino): Update this to a filtered lookup on clusterName once that graphql
// endpoint has landed. PC-471.
const GET_CLUSTER_CONTROL_PLANE_PODS = gql`
{
  clusters {
    clusterName
    controlPlanePodStatuses {
      name
      status
      message
      reason
    }
  }
}
`;

const formatPodStatus = ({
  name, status, message, reason,
}) => ({
  name,
  status,
  message,
  reason,
  statusGroup: podStatusGroup(status),
});

const ControlPlanePodsTable = ({ selectedClusterName }) => {
  const { loading, data } = useQuery(GET_CLUSTER_CONTROL_PLANE_PODS);
  if (loading) {
    return <div>Loading</div>;
  }
  const cluster = data?.clusters.find((cluster) => cluster.clusterName === selectedClusterName);
  if (!cluster) {
    return (
      <div>
        Cluster
        {' '}
        {selectedClusterName}
        {' '}
        not found.
      </div>
    );
  }
  const display = cluster.controlPlanePodStatuses.map((podStatus) => formatPodStatus(podStatus));
  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell />
          <StyledTableHeaderCell>Name</StyledTableHeaderCell>
          <StyledTableHeaderCell>Message</StyledTableHeaderCell>
          <StyledTableHeaderCell>Reason</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {display.map((podStatus) => (
          <TableRow key={podStatus.name}>
            <AdminTooltip title={podStatus.status}>
              <StyledLeftTableCell>
                <StatusCell statusGroup={podStatus.statusGroup} />
              </StyledLeftTableCell>
            </AdminTooltip>
            <StyledTableCell>{podStatus.name}</StyledTableCell>
            <StyledTableCell>{podStatus.message}</StyledTableCell>
            <StyledRightTableCell>{podStatus.reason}</StyledRightTableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
};

const LIST_CLUSTERS = gql`
{
  clusters {
    id
    status
    clusterName
    prettyClusterName
    vizierConfig {
      passthroughEnabled
    }
  }
}
`;

const ClusterDetailsNavigation = ({ selectedClusterName }) => {
  const history = useHistory();
  const { loading, data } = useQuery(LIST_CLUSTERS);

  if (loading) {
    return (<div>Loading...</div>);
  }
  // Cluster always goes first in breadcrumbs.
  const clusterPrettyNameToFullName = {};
  let selectedClusterPrettyName = 'unknown cluster';

  data.clusters.forEach(({ prettyClusterName, clusterName }) => {
    clusterPrettyNameToFullName[prettyClusterName] = clusterName;
    if (clusterName === selectedClusterName) {
      selectedClusterPrettyName = prettyClusterName;
    }
  });

  const breadcrumbs = [{
    title: 'clusterName',
    value: selectedClusterPrettyName,
    selectable: true,
    omitKey: true,
    // eslint-disable-next-line
    getListItems: async (input) => (data.clusters.filter((c) => c.status !== CLUSTER_STATUS_DISCONNECTED)
      .map((c) => ({ value: c.prettyClusterName }))
    ),
    onSelect: (input) => {
      history.push(getClusterDetailsURL(clusterPrettyNameToFullName[input]));
    },
  }];
  return (
    <StyledBreadcrumbs>
      <StyledBreadcrumbLink to='/admin'>Admin</StyledBreadcrumbLink>
      <StyledBreadcrumbLink to='/admin'>Clusters</StyledBreadcrumbLink>
      <PixieBreadcrumbs breadcrumbs={breadcrumbs} />
    </StyledBreadcrumbs>
  );
};

export const ClusterDetails = withStyles((theme: Theme) => ({
  error: {
    padding: 20,
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  container: {
    maxHeight: 800,
  },
}))(({ classes }: any) => {
  const { name } = useParams();
  const clusterName = decodeURIComponent(name);

  const [tab, setTab] = React.useState('agents');
  const { loading, error, data } = useQuery(LIST_CLUSTERS, { pollInterval: AGENTS_POLL_INTERVAL });

  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }
  if (!data || !data.clusters) {
    return <div className={classes.error}>No clusters found.</div>;
  }

  const cluster = data.clusters.find((c) => c.clusterName === clusterName);
  if (!cluster) {
    return (
      <>
        <ClusterDetailsNavigation selectedClusterName={clusterName} />
        <div className={classes.error}>
          Cluster
          {' '}
          {name}
          {' '}
          not found.
        </div>
      </>
    );
  }

  return (
    <div>
      <ClusterDetailsNavigation selectedClusterName={clusterName} />
      <StyledTabs
        value={tab}
        onChange={(event, newTab) => setTab(newTab)}
      >
        <StyledTab value='agents' label='Agents' />
        <StyledTab value='control-plane-pods' label='Control Plane Pods' />
      </StyledTabs>
      <div className={classes.tabContents}>
        {
          tab === 'agents'
          && (
            <VizierGRPCClientProvider
              clusterID={cluster.id}
              passthroughEnabled={cluster.vizierConfig.passthroughEnabled}
              clusterStatus={cluster.status}
            >
              <TableContainer className={classes.container}>
                <AgentsTable />
              </TableContainer>
            </VizierGRPCClientProvider>
          )
        }
        {
          tab === 'control-plane-pods'
          && (
            <TableContainer className={classes.container}>
              <ControlPlanePodsTable selectedClusterName={clusterName} />
            </TableContainer>
          )
        }
      </div>
    </div>
  );
});
