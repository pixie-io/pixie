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
import {
  Help as HelpIcon,
  KeyboardArrowDown as DownIcon,
  KeyboardArrowUp as UpIcon,
} from '@mui/icons-material';
import {
  Breadcrumbs as MaterialBreadcrumbs,
  IconButton,
  Table,
  TableBody,
  TableContainer,
  TableCell,
  TableHead,
  TableRow,
} from '@mui/material';
import { Theme, styled } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { distanceInWords } from 'date-fns';
import { useHistory, useParams } from 'react-router';
import { Link } from 'react-router-dom';
import { BehaviorSubject } from 'rxjs';
import { filter, tap } from 'rxjs/operators';

import { ExecutionStateUpdate, PixieAPIContext, VizierQueryResult } from 'app/api';
import { ClusterContext, ClusterContextProps, useClusterConfig } from 'app/common/cluster-context';
import { Breadcrumbs, StatusCell, StatusGroup } from 'app/components';
import {
  GQLClusterInfo,
  GQLClusterStatus as ClusterStatus,
  GQLPodStatus as PodStatus,
  GQLContainerStatus as ContainerStatus,
  GQLPodStatus,
} from 'app/types/schema';
import { dataFromProto } from 'app/utils/result-data-utils';

import {
  ClusterStatusCell, InstrumentationLevelCell, VizierVersionCell, MonoSpaceCell,
} from './cluster-table-cells';
import {
  AdminTooltip, agentStatusGroup, clusterStatusGroup, containerStatusGroup,
  convertHeartbeatMS, getClusterDetailsURL, podStatusGroup, StyledLeftTableCell,
  StyledRightTableCell,
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
}), { name: 'BreadcrumbLink' });
const StyledBreadcrumbLink: React.FC<{ to: string }> = React.memo(({ children, to }) => {
  const classes = useLinkStyles();
  return <Link className={classes.root} to={to}>{children}</Link>;
});
StyledBreadcrumbLink.displayName = 'StyledBreadcrumbLink';

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
}), { name: 'ClusterDetailsBreadcrumbs' });

const StyledBreadcrumbs: React.FC = React.memo(({ children }) => {
  const classes = useBreadcrumbsStyles();
  return (
    <MaterialBreadcrumbs classes={classes}>
      {children}
    </MaterialBreadcrumbs>
  );
});
StyledBreadcrumbs.displayName = 'StyledBreadcrumbs';

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

// TODO(nick,PC-1246): After MUI stuff is done, come back and fix the lint warnings here instead of suppressing.
/* eslint-disable react-memo/require-memo,react-memo/require-usememo */

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
AgentsTableContent.displayName = 'AgentsTableContent';

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
          const data = dataFromProto(results.tables[0].relation, results.tables[0].batches);
          setState({ data });
        }
      };
      const onError = (error) => {
        setState({ data: [], error: error?.message });
      };
      client.executeScript(clusterConfig, AGENT_STATUS_SCRIPT, { enableE2EEncryption: true }, []).pipe(
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
AgentsTable.displayName = 'AgentsTable';

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

const usePodRowStyles = makeStyles((theme: Theme) => createStyles({
  messageAndReason: {
    ...theme.typography.body2,
  },
  eventList: {
    marginTop: 0,
  },
  smallTable: {
    backgroundColor: theme.palette.foreground.grey3,
  },
}));

const StyledSmallTableCell = styled(StyledTableCell, { name: 'SmallTableCell' })(({ theme }) => ({
  fontWeight: theme.typography.fontWeightLight,
  backgroundColor: theme.palette.foreground.grey2,
  borderWidth: 0,
}));

// combineReasonAndMessage returns a combination of a reason and a message. These fields are
// used by Kubernetes to detail state However, we don't need to separate them when we
// display to the user. We would rather combine them together.
function combineReasonAndMessage(reason: string, message: string): string {
  // If both are defined we want to separate them with a semicolon
  if (message && reason) {
    return `${message}; ${reason}`;
  }
  return `${message}${reason}`;
}

const ExpandablePodRow: React.FC<{ podStatus: GroupedPodStatus }> = (({ podStatus }) => {
  const {
    name, status, statusGroup, containers, events,
  } = podStatus;
  const [open, setOpen] = React.useState(false);
  const classes = usePodRowStyles();

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
        <StyledTableCell>{combineReasonAndMessage(podStatus.reason, podStatus.message)}</StyledTableCell>
        <StyledTableCell>{podStatus.restartCount}</StyledTableCell>
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
              <Table size='small' className={classes.smallTable}>
                <TableHead>
                  <TableRow key={name}>
                    <StyledTableHeaderCell />
                    <StyledTableHeaderCell>Container</StyledTableHeaderCell>
                    <StyledTableHeaderCell>Status</StyledTableHeaderCell>
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
                          <StyledSmallTableCell>
                            <StatusCell statusGroup={container.statusGroup} />
                          </StyledSmallTableCell>
                        </AdminTooltip>
                        <StyledSmallTableCell>{container.name}</StyledSmallTableCell>
                        <StyledSmallTableCell>
                          {combineReasonAndMessage(container.reason, container.message)}
                        </StyledSmallTableCell>
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
ExpandablePodRow.displayName = 'ExpandablePodRow';

const useClusterDetailStyles = makeStyles((theme: Theme) => createStyles({
  errorMessage: {
    ...theme.typography.body1,
    padding: theme.spacing(3),
  },
  tabContents: {
    margin: theme.spacing(1),
  },
  tableContainer: {
    maxHeight: theme.spacing(100),
  },
  podTypeHeader: {
    ...theme.typography.h6,
    color: theme.palette.foreground.grey5,
    alignItems: 'center',
    height: '100%',
    paddingLeft: theme.spacing(2),
  },
  topPadding: {
    paddingTop: theme.spacing(2),
    paddingBottom: theme.spacing(2),
  },
  titleContainer: {
    display: 'flex',
  },
  helpIcon: {
    display: 'flex',
    paddingLeft: theme.spacing(1),
  },
  detailsTable: {
    maxWidth: theme.breakpoints.values.md,
  },
}), { name: 'ClusterDetail' });

const PodHeader = () => (
  <TableHead>
    <TableRow>
      <StyledTableHeaderCell />
      <StyledTableHeaderCell>Name</StyledTableHeaderCell>
      <StyledTableHeaderCell>Status</StyledTableHeaderCell>
      <StyledTableHeaderCell>Restart Count</StyledTableHeaderCell>
      <StyledTableHeaderCell />
    </TableRow>
  </TableHead>
);
PodHeader.displayName = 'PodHeader';

const PixiePodsTab: React.FC<{
  controlPlanePods: GQLPodStatus[],
  dataPlanePods: GQLPodStatus[],
}> = ({ controlPlanePods, dataPlanePods }) => {
  const classes = useClusterDetailStyles();
  const controlPlaneDisplay = controlPlanePods.map((podStatus) => formatPodStatus(podStatus));
  const dataPlaneDisplay = dataPlanePods.map((podStatus) => formatPodStatus(podStatus));

  return (
    <>
      <div className={`${classes.podTypeHeader} ${classes.topPadding}`}> Control Plane Pods </div>
      <Table>
        <PodHeader />
        <TableBody>
          {controlPlaneDisplay.map((podStatus) => (
            <ExpandablePodRow key={podStatus.name} podStatus={podStatus} />
          ))}
        </TableBody>
        {/* We place this header as a row so our data plane pods are aligned with the control plane pods.
        We also make the column span 3 so it sizes with the State, Name, and Status column. Without
        this specification, the cell would default to colspan="1" which would cause the state column
        to be extremely wide. If you ever reduce the number of columns in the table you'll want to reduce
        this value.
        */}
        <TableRow>
          <TableCell className={classes.podTypeHeader} colSpan={3}>
            <div className={classes.titleContainer}>
              <div className={classes.titleContainer}>
                Sample of Unhealthy Data Plane Pods
            </div>
              <AdminTooltip title={'Sample of unhealthy Pixie data plane pods. '
                + 'To see a list of all agents, click the Agents tab.'}>
                <HelpIcon className={classes.helpIcon} />
              </AdminTooltip>
            </div>
          </TableCell>
        </TableRow>
        <PodHeader />
        <TableBody>
          {
            dataPlanePods?.length > 0
              ? dataPlaneDisplay.map((podStatus) => (
                <ExpandablePodRow key={podStatus.name} podStatus={podStatus} />
              )) : <div className={classes.errorMessage}> Cluster has no unhealthy Pixie data plane pods. </div>
          }
        </TableBody>
      </Table>
    </>
  );
};
PixiePodsTab.displayName = 'PixiePodsTab';

const AgentsTab: React.FC<{
  cluster: Pick<GQLClusterInfo, 'id' | 'clusterName' | 'status' | 'unhealthyDataPlanePodStatuses'>
}> = ({ cluster }) => {
  const classes = useClusterDetailStyles();
  const statusGroup = clusterStatusGroup(cluster.status);

  return (
    <>
      <TableContainer className={classes.tableContainer}>
        {(statusGroup !== 'healthy' && statusGroup !== 'degraded') ? (
          <div className={classes.errorMessage}>
            {`Cannot get agents for cluster ${cluster.clusterName}, reason: ${statusGroup}`}
          </div>
        )
          : (<AgentsTable />)
        }
      </TableContainer>
    </>
  );
};
AgentsTab.displayName = 'AgentsTab';

const ClusterSummaryTable = ({ cluster }: {
  cluster: Pick<
  GQLClusterInfo,
  'clusterName' |
  'id' |
  'status' |
  'statusMessage' |
  'numNodes' |
  'numInstrumentedNodes' |
  'vizierVersion' |
  'clusterVersion' |
  'lastHeartbeatMs' |
  'vizierConfig'
  >
}) => {
  const classes = useClusterDetailStyles();
  if (!cluster) {
    return (
      <div>
        Cluster not found.
      </div>
    );
  }

  const data = [
    {
      key: 'Name',
      value: cluster.clusterName,
    },
    {
      key: 'ID',
      value: (<MonoSpaceCell data={cluster.id} />),
    },
    {
      key: 'Status',
      value: (<ClusterStatusCell status={cluster.status} message={cluster.statusMessage} />),
    },
    {
      key: 'Instrumented Nodes',
      value: (
        <InstrumentationLevelCell cluster={cluster} />
      ),
    },
    {
      key: 'Vizier Version',
      value: (<VizierVersionCell version={cluster.vizierVersion} />),
    },
    {
      key: 'Kubernetes Version',
      value: cluster.clusterVersion,
    },
    {
      key: 'Heartbeat',
      value: convertHeartbeatMS(cluster.lastHeartbeatMs),
    },
    {
      key: 'Data Mode',
      value: cluster.vizierConfig.passthroughEnabled ? 'Passthrough' : 'Direct',
    },
  ];

  return (
    <TableContainer className={classes.tableContainer}>
      <div className={classes.detailsTable}>
        <Table>
          <TableBody>
            {data.map((r) => (
              <TableRow key={r.key}>
                <StyledTableCell >
                  {r.key}
                </StyledTableCell>
                {!React.isValidElement(r.value)
                  ? <StyledTableCell>{r.value}</StyledTableCell>
                  : r.value
                }
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </div >
    </TableContainer>
  );
};
ClusterSummaryTable.displayName = 'ClusterSummaryTable';

const ClusterDetailsNavigationBreadcrumbs = ({ selectedClusterName }) => {
  const history = useHistory();
  const { data, loading, error } = useQuery<{
    clusters: Pick<GQLClusterInfo, 'clusterName' | 'prettyClusterName' | 'status'>[],
  }>(gql`
    query clusterNavigationData{
      clusters {
        id
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
    getListItems: async (input) => {
      const items = clusters
        .filter((c) => c.status !== ClusterStatus.CS_DISCONNECTED && c.prettyClusterName.indexOf(input) >= 0)
        .map((c) => ({ value: c.prettyClusterName }));
      return { items, hasMoreItems: false };
    },
    onSelect: (input) => {
      history.push(getClusterDetailsURL(clusterPrettyNameToFullName[input]));
    },
  }];
  return (<Breadcrumbs breadcrumbs={breadcrumbs} />);
};
ClusterDetailsNavigationBreadcrumbs.displayName = 'ClusterDetailsNavigationBreadcrumbs';

const ClusterDetailsTabs: React.FC<{ clusterName: string }> = ({ clusterName }) => {
  const classes = useClusterDetailStyles();
  const [tab, setTab] = React.useState('details');

  const { data, loading, error } = useQuery<{
    clusterByName: Pick<
    GQLClusterInfo,
    'id' |
    'clusterName' |
    'clusterVersion' |
    'vizierVersion' |
    'prettyClusterName' |
    'clusterUID' |
    'vizierConfig' |
    'status' |
    'statusMessage' |
    'controlPlanePodStatuses' |
    'unhealthyDataPlanePodStatuses' |
    'lastHeartbeatMs' |
    'numNodes' |
    'numInstrumentedNodes'
    >
  }>(
    gql`
      query GetClusterByName($name: String!) {
        clusterByName(name: $name) {
          id
          clusterName
          prettyClusterName
          status
          statusMessage
          clusterVersion
          vizierVersion
          vizierConfig {
            passthroughEnabled
          }
          lastHeartbeatMs
          numNodes
          numInstrumentedNodes
          controlPlanePodStatuses {
            name
            status
            message
            reason
            restartCount
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
          unhealthyDataPlanePodStatuses {
            name
            status
            message
            reason
            restartCount
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
      }`,
    // Ignore cache on first fetch, to avoid blinking stale heartbeats.
    {
      variables: { name: clusterName },
      pollInterval: 60000,
      fetchPolicy: 'network-only',
      nextFetchPolicy: 'cache-first',
    },
  );

  const cluster = data?.clusterByName;

  const clusterContext: ClusterContextProps = React.useMemo(() => (cluster && {
    loading: false,
    selectedClusterID: cluster?.id,
    selectedClusterName: cluster?.clusterName,
    selectedClusterPrettyName: cluster?.prettyClusterName,
    selectedClusterUID: cluster?.clusterUID,
    selectedClusterVizierConfig: cluster?.vizierConfig,
    selectedClusterStatus: cluster?.status,
    selectedClusterStatusMessage: cluster?.statusMessage,
    setClusterByName: () => {},
  }), [cluster]);

  if (loading) {
    return <div className={classes.errorMessage}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.errorMessage}>{error.toString()}</div>;
  }

  if (!cluster) {
    return (
      <>
        <div className={classes.errorMessage}>
          Cluster
          {' '}
          {clusterName}
          {' '}
          not found.
        </div>
      </>
    );
  }

  return (
    <>
      <StyledTabs
        value={tab}
        onChange={(event, newTab) => setTab(newTab)}
      >
        <StyledTab value='details' label='Details' />
        <StyledTab value='agents' label='Agents' />
        <StyledTab value='pixie-pods' label='Pixie Pods' />
      </StyledTabs>
      <div className={classes.tabContents}>
        {
          tab === 'details' && (
            <ClusterSummaryTable cluster={cluster} />
          )
        }
        {
          tab === 'agents' && (
            <ClusterContext.Provider value={clusterContext}>
              <AgentsTab cluster={cluster} />
            </ClusterContext.Provider>
          )
        }
        {
          tab === 'pixie-pods' && (
            <PixiePodsTab
              controlPlanePods={cluster.controlPlanePodStatuses}
              dataPlanePods={cluster.unhealthyDataPlanePodStatuses}
            />
          )
        }
      </div>
    </>
  );
};
ClusterDetailsTabs.displayName = 'ClusterDetailsTabs';

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
ClusterDetails.displayName = 'ClusterDetails';
