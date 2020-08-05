import * as React from 'react';

import ClientContext, {
  VizierGRPCClientProvider, CLUSTER_STATUS_DISCONNECTED,
} from 'common/vizier-grpc-client-context';
import PixieBreadcrumbs from 'components/breadcrumbs/breadcrumbs';
import { StatusCell, StatusGroup } from 'components/status/status';
import { distanceInWords } from 'date-fns';
import gql from 'graphql-tag';
import { useHistory, useParams } from 'react-router';
import { Link } from 'react-router-dom';
import { dataFromProto } from 'utils/result-data-utils';

import { useQuery } from '@apollo/react-hooks';

import { Theme, withStyles } from '@material-ui/core/styles';
import Breadcrumbs from '@material-ui/core/Breadcrumbs';
import IconButton from '@material-ui/core/IconButton';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableCell from '@material-ui/core/TableCell';
import TableContainer from '@material-ui/core/TableContainer';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';

import {
  AdminTooltip, agentStatusGroup, clusterStatusGroup, containerStatusGroup,
  convertHeartbeatMS, getClusterDetailsURL, podStatusGroup, StyledLeftTableCell,
  StyledRightTableCell, StyledSmallLeftTableCell, StyledSmallRightTableCell,
  StyledTab, StyledTableCell, StyledTableHeaderCell, StyledTabs,
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
      client.executeScript(AGENT_STATUS_SCRIPT, [], false).then((results) => {
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
      containers {
        name
        state
        reason
        message
      }
    }
  }
}
`;

const formatPodStatus = ({
  name, status, message, reason, containers,
}) => ({
  name,
  status,
  message,
  reason,
  statusGroup: podStatusGroup(status),
  containers: containers.map((container) => ({
    name: container.name,
    state: container.state,
    message: container.message,
    reason: container.reason,
    statusGroup: containerStatusGroup(container.state),
  })),
});

const none = '<none>';

const ExpandablePodRow = withStyles((theme: Theme) => ({
  messageAndReason: {
    ...theme.typography.body2,
  },
}))(({ podStatus, classes }: any) => {
  const {
    name, status, statusGroup, message, reason, containers,
  } = podStatus;
  const [open, setOpen] = React.useState(false);

  return (
    // Fragment shorthand syntax does not support key, which is needed to prevent
    // the console error where a key is not present in a list element.
    // eslint-disable-next-line react/jsx-fragments
    <React.Fragment key={name}>
      <TableRow key={name}>
        <AdminTooltip title={status}>
          <StyledLeftTableCell>
            <StatusCell statusGroup={statusGroup} />
          </StyledLeftTableCell>
        </AdminTooltip>
        <StyledTableCell>{name}</StyledTableCell>
        <StyledRightTableCell align='right'>
          <IconButton size='small' onClick={() => setOpen(!open)}>
            {open ? <UpIcon /> : <DownIcon />}
          </IconButton>
        </StyledRightTableCell>
      </TableRow>
      {
        open && (
          <TableRow key={`${name}-details`}>
            <TableCell style={{ border: 0 }} colSpan={6}>
              <div className={classes.messageAndReason}>
                Pod message:
                {' '}
                {message || none}
              </div>
              <div className={classes.messageAndReason}>
                Pod reason:
                {' '}
                {reason || none}
              </div>
              <Table size='small'>
                <TableHead>
                  <TableRow key={name}>
                    <StyledTableHeaderCell />
                    <StyledTableHeaderCell>Container</StyledTableHeaderCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {containers.map((container) => (
                    // Fragment shorthand syntax does not support key, which is needed to prevent
                    // the console error where a key is not present in a list element.
                    // eslint-disable-next-line react/jsx-fragments
                    <React.Fragment key={container.name}>
                      <TableRow key={`${name}-info`}>
                        <AdminTooltip title={container.state}>
                          <StyledSmallLeftTableCell>
                            <StatusCell statusGroup={container.statusGroup} />
                          </StyledSmallLeftTableCell>
                        </AdminTooltip>
                        <StyledSmallRightTableCell>{container.name}</StyledSmallRightTableCell>
                      </TableRow>
                      <TableRow key={`${name}-details`}>
                        <TableCell style={{ border: 0 }} colSpan={2}>
                          <div className={classes.messageAndReason}>
                            Container message:
                            {' '}
                            {container.message || none}
                          </div>
                          <div className={classes.messageAndReason}>
                            Container reason:
                            {' '}
                            {container.reason || none}
                          </div>
                        </TableCell>
                      </TableRow>
                    </React.Fragment>
                  ))}
                </TableBody>
              </Table>
            </TableCell>
          </TableRow>
        )
      }
    </React.Fragment>
  );
});

const ControlPlanePodsTable = ({ selectedClusterName }) => {
  const { loading, data } = useQuery(GET_CLUSTER_CONTROL_PLANE_PODS);
  if (loading) {
    return <div>Loading</div>;
  }
  const cluster = data?.clusters.find((c) => c.clusterName === selectedClusterName);
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
          <StyledTableHeaderCell />
        </TableRow>
      </TableHead>
      <TableBody>
        {display.map((podStatus) => (
          <ExpandablePodRow key={podStatus.name} podStatus={podStatus} />
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
    ...theme.typography.body1,
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

  const statusGroup = clusterStatusGroup(cluster.status);

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
          tab === 'agents' && (
            statusGroup === 'healthy' ? (
              <VizierGRPCClientProvider
                clusterID={cluster.id}
                passthroughEnabled={cluster.vizierConfig.passthroughEnabled}
                clusterStatus={cluster.status}
              >
                <TableContainer className={classes.container}>
                  <AgentsTable />
                </TableContainer>
              </VizierGRPCClientProvider>
            ) : (
              <div className={classes.error}>
                Cannot get agents for cluster
                {' '}
                {clusterName}
                {', '}
                cluster is in state
                {': '}
                {cluster.status.replace('CS_', '')}
              </div>
            )
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
