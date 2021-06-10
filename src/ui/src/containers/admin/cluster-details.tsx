/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import { gql, useQuery } from '@apollo/client';
import { Breadcrumbs, StatusCell, StatusGroup } from 'app/components';
import { distanceInWords } from 'date-fns';
import { useHistory, useParams } from 'react-router';
import { Link } from 'react-router-dom';
import { dataFromProto } from 'app/utils/result-data-utils';

import { Theme, makeStyles } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
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

import { ExecutionStateUpdate, PixieAPIContext, VizierQueryResult } from 'app/api';
import {
  GQLClusterInfo,
  GQLClusterStatus as ClusterStatus,
  GQLPodStatus as PodStatus,
  GQLContainerStatus as ContainerStatus,
} from 'app/types/schema';

import { BehaviorSubject } from 'rxjs';
import { filter, tap } from 'rxjs/operators';
import { ClusterContext, ClusterContextProps, useClusterConfig } from 'app/common/cluster-context';
import {
  AdminTooltip, agentStatusGroup, clusterStatusGroup, containerStatusGroup,
  convertHeartbeatMS, getClusterDetailsURL, podStatusGroup, StyledLeftTableCell,
  StyledRightTableCell, StyledSmallLeftTableCell, StyledSmallRightTableCell,
  StyledTab, StyledTableCell, StyledTableHeaderCell, StyledTabs,
} from './utils';

const useLinkStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    ...theme.typography.body2,
    display: 'flex',
    alignItems: 'center',
    paddingLeft: theme.spacing(1),
    paddingRight: theme.spacing(1),
    height: theme.spacing(3),
    color: theme.palette.foreground.grey5,
  },
}));
const StyledBreadcrumbLink: React.FC<{ to: string }> = (({ children, to }) => {
  const classes = useLinkStyles();
  return <Link className={classes.root} to={to}>{children}</Link>;
});

const useBreadcrumbsStyles = makeStyles((theme: Theme) => createStyles({
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
}));
const StyledBreadcrumbs: React.FC = ({ children }) => {
  const classes = useBreadcrumbsStyles();
  return (
    <MaterialBreadcrumbs classes={classes}>
      {children}
    </MaterialBreadcrumbs>
  );
};

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

interface AgentDisplayState {
  error?: string;
  data: Array<unknown>;
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
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

const AgentsTable: React.FC = () => {
  const clusterConfig = useClusterConfig();
  const client = React.useContext(PixieAPIContext);

  const [state, setState] = React.useState<AgentDisplayState>({ data: [] });

  React.useEffect(() => {
    if (!client) {
      return () => {
      }; // noop
    }
    const executionSubject = new BehaviorSubject<ExecutionStateUpdate | null>(null);
    const fetchAgentStatus = () => {
      const onResults = (results: VizierQueryResult) => {
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
      client.executeScript(clusterConfig, AGENT_STATUS_SCRIPT, []).pipe(
        filter((update) => !['data', 'cancel', 'error'].includes(update.event.type)),
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
  }, [client, clusterConfig]);

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

interface GroupedPodStatus extends Omit<PodStatus, 'containers'> {
  statusGroup: StatusGroup;
  containers: Array<ContainerStatus & { statusGroup: StatusGroup }>;
}

const formatPodStatus = (podStatus: PodStatus): GroupedPodStatus => ({
  ...podStatus,
  statusGroup: podStatusGroup(podStatus.status),
  containers: podStatus.containers.map((container) => ({
    ...container,
    statusGroup: containerStatusGroup(container.state),
  })),
});

const none = '<none>';

const useRowStyles = makeStyles((theme: Theme) => createStyles({
  messageAndReason: {
    ...theme.typography.body2,
  },
  eventList: {
    marginTop: 0,
  },
}));

const ExpandablePodRow: React.FC<{ podStatus: GroupedPodStatus }> = (({ podStatus }) => {
  const {
    name, status, statusGroup, message, reason, containers, events,
  } = podStatus;
  const [open, setOpen] = React.useState(false);
  const classes = useRowStyles();

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

const ControlPlanePodsTable: React.FC<{
  cluster: Pick<GQLClusterInfo, 'id' | 'clusterName' | 'controlPlanePodStatuses'>
}> = ({ cluster }) => {
  if (!cluster) {
    return (
      <div>
        Cluster not found.
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

const ClusterDetailsNavigationBreadcrumbs = ({ selectedClusterName }) => {
  const history = useHistory();
  const { data, loading, error } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'prettyClusterName' | 'status'>[],
  }>(gql`
        query clusterNavigationData{
            clusters {
                clusterName
                prettyClusterName
                status
            }
        }
    `, {});
  const clusters = data?.clusters;

  if (loading || error || !clusters) {
    return (<Breadcrumbs breadcrumbs={[]} />);
  }

  // Cluster always goes first in breadcrumbs.
  const clusterPrettyNameToFullName = {};
  let selectedClusterPrettyName = 'unknown cluster';

  clusters.forEach(({ prettyClusterName, clusterName }) => {
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
    getListItems: async () => (clusters.filter((c) => c.status !== ClusterStatus.CS_DISCONNECTED)
      .map((c) => ({ value: c.prettyClusterName }))
    ),
    onSelect: (input) => {
      history.push(getClusterDetailsURL(clusterPrettyNameToFullName[input]));
    },
  }];
  return (<Breadcrumbs breadcrumbs={breadcrumbs} />);
};

const useClusterDetailStyles = makeStyles((theme: Theme) => createStyles({
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
}));

const ClusterDetailsTabs: React.FC<{ clusterName: string }> = ({ clusterName }) => {
  const classes = useClusterDetailStyles();
  const [tab, setTab] = React.useState('agents');

  const { data, loading, error } = useQuery<{
    clusterByName: Pick<
    GQLClusterInfo,
    'id' | 'clusterName' | 'prettyClusterName' | 'clusterUID' | 'vizierConfig' | 'status' | 'controlPlanePodStatuses'
    >
  }>(
    gql`
      query GetClusterByName($name: String!) {
        clusterByName(name: $name) {
          id
          clusterName
          prettyClusterName
          status
          vizierConfig {
            passthroughEnabled
          }
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
      }`, { variables: { name: clusterName } },
  );

  const cluster = data?.clusterByName;

  const clusterContext: ClusterContextProps = React.useMemo(() => (cluster && {
    selectedClusterID: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    selectedClusterVizierConfig: cluster?.vizierConfig,
    selectedClusterStatus: cluster?.status,
    setClusterByName: () => {},
  }), [cluster]);

  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  if (!cluster) {
    return (
      <>
        <div className={classes.error}>
          Cluster
          {' '}
          {clusterName}
          {' '}
          not found.
        </div>
      </>
    );
  }

  const statusGroup = clusterStatusGroup(cluster.status);

  return (
    <>
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
              <ClusterContext.Provider value={clusterContext}>
                <TableContainer className={classes.container}>
                  <AgentsTable />
                </TableContainer>
              </ClusterContext.Provider>
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
              <ControlPlanePodsTable cluster={cluster} />
            </TableContainer>
          )
        }
      </div>
    </>
  );
};

export const ClusterDetails: React.FC = () => {
  const { name } = useParams<{ name: string }>();
  const clusterName = decodeURIComponent(name);

  return (
    <div>
      <StyledBreadcrumbs>
        <StyledBreadcrumbLink to='/admin'>Admin</StyledBreadcrumbLink>
        <StyledBreadcrumbLink to='/admin'>Clusters</StyledBreadcrumbLink>
        <ClusterDetailsNavigationBreadcrumbs selectedClusterName={clusterName} />
      </StyledBreadcrumbs>
      <ClusterDetailsTabs clusterName={clusterName} />
    </div>
  );
};
