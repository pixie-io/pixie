import * as React from 'react';

import ClientContext, {
  VizierGRPCClientProvider, CLUSTER_STATUS_DISCONNECTED,
} from 'common/vizier-grpc-client-context';
import { Breadcrumbs, StatusCell, StatusGroup } from 'pixie-components';
import { distanceInWords } from 'date-fns';
import gql from 'graphql-tag';
import { useHistory, useParams } from 'react-router';
import { Link } from 'react-router-dom';
import { dataFromProto } from 'utils/result-data-utils';

import { useQuery } from '@apollo/react-hooks';

import { Theme, withStyles } from '@material-ui/core/styles';
import MaterialBreadcrumbs from '@material-ui/core/Breadcrumbs';
import IconButton from '@material-ui/core/IconButton';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableCell from '@material-ui/core/TableCell';
import TableContainer from '@material-ui/core/TableContainer';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';

import { ExecutionStateUpdate } from 'pixie-api';
import { BehaviorSubject } from 'rxjs';
import { filter, tap } from 'rxjs/operators';
import {
  AdminTooltip, agentStatusGroup, clusterStatusGroup, containerStatusGroup,
  convertHeartbeatMS, getClusterDetailsURL, podStatusGroup, StyledLeftTableCell,
  StyledRightTableCell, StyledSmallLeftTableCell, StyledSmallRightTableCell,
  StyledTab, StyledTableCell, StyledTableHeaderCell, StyledTabs,
} from './utils';

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
  <MaterialBreadcrumbs classes={classes}>
    {children}
  </MaterialBreadcrumbs>
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
  const agentID = agentInfo.agent_id;
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
      return () => { }; // noop
    }
    const executionSubject = new BehaviorSubject<ExecutionStateUpdate|null>(null);
    const fetchAgentStatus = () => {
      const onResults = (results) => {
        if (!results.schemaOnly) {
          if (results.tables.length !== 1) {
            if (results.status) {
              setState({ data: [], error: results.status.getMessage() });
            }
            return;
          }
          const data = dataFromProto(results.tables[0].relation, results.tables[0].data);
          setState({ data });
        }
      };
      const onError = (error) => {
        setState({ data: [], error: error?.message });
      };
      client.executeScript(AGENT_STATUS_SCRIPT, [], false).pipe(
        filter((update) => !['data', 'cancel'].includes(update.event.type)),
        tap((update) => {
          if (update.event.type === 'error') {
            onError(update.event.error);
          } else {
            onResults(update.results);
          }
        }),
      ).subscribe(executionSubject);
    };

    fetchAgentStatus(); // Fetch the agent status initially, before starting the timer for AGENTS_POLL_INTERVAL.
    const interval = setInterval(() => {
      fetchAgentStatus();
    }, AGENTS_POLL_INTERVAL);
    return () => {
      clearInterval(interval);
      if (executionSubject.value?.cancel) {
        executionSubject.value?.cancel();
      }
      executionSubject.unsubscribe();
    };
  }, [client]);

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
      events {
        message
      }
    }
  }
}
`;

const formatPodStatus = ({
  name, status, message, reason, containers, events,
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
  events: events.map((event) => ({
    message: event.message,
  })),
});

const none = '<none>';

const ExpandablePodRow = withStyles((theme: Theme) => ({
  messageAndReason: {
    ...theme.typography.body2,
  },
  eventList: {
    marginTop: 0,
  },
}))(({ podStatus, classes }: any) => {
  const {
    name, status, statusGroup, message, reason, containers, events,
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
              {
                (events && events.length > 0) && (
                  <div>
                    Events:
                    <ul className={classes.eventList}>
                      {events.map((event, i) => (
                        <li key={`${name}-${i}`}>
                          {event.message}
                        </li>
                      ))}
                    </ul>
                  </div>
                )
              }
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
    getListItems: async () => (data.clusters.filter((c) => c.status !== CLUSTER_STATUS_DISCONNECTED)
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
      <Breadcrumbs breadcrumbs={breadcrumbs} />
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
