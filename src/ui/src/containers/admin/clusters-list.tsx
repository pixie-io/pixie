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
import { Close as CloseIcon } from '@mui/icons-material';
import {
  Button,
  Card,
  IconButton,
  Modal,
  Table,
  TableBody,
  TableHead,
  TableRow,
  Tooltip,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Link, Route, RouteComponentProps, Switch, useRouteMatch } from 'react-router-dom';

import { Spinner } from 'app/components';
import { ClusterDetails } from 'app/containers/admin/cluster-details';
import { GQLClusterInfo } from 'app/types/schema';

import {
  ClusterStatusCell, InstrumentationLevelCell, VizierVersionCell, MonoSpaceCell,
} from './cluster-table-cells';
import {
  getClusterDetailsURL,
  StyledTableCell,
  StyledTableHeaderCell,
} from './utils';

// Cluster that are older that has not been healthy in over a day are labelled inactive.
const INACTIVE_CLUSTER_THRESHOLD_MS = 24 * 60 * 60 * 1000;

type ClusterRowInfo = Pick<GQLClusterInfo,
'id' |
'clusterName' |
'prettyClusterName' |
'vizierVersion' |
'status' |
'numNodes' |
'numInstrumentedNodes' |
'lastHeartbeatMs'>;

const useStyles = makeStyles((theme: Theme) => createStyles({
  modalRoot: {
    width: '90vw',
    maxWidth: theme.breakpoints.values.lg,
    position: 'absolute',
    top: '50%',
    left: '50%',
    transform: 'translate(-50%, -50%)',
    // A consistent height, even if most of it is unused in some tabs, avoids controls jumping around the screen.
    height: `calc(min(90vh, ${theme.breakpoints.values.md}px))`,
    overflow: 'auto',
  },
  error: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'center',
    alignItems: 'center',
    height: '100%',
    width: '100%',
    padding: theme.spacing(1),
  },
  removePadding: {
    padding: 0,
  },
  root: {
    position: 'relative',
    width: '100%',
    maxWidth: theme.breakpoints.values.lg,
    margin: '0 auto',
  },
  loadingIndicator: {
    position: 'absolute',
    bottom: theme.spacing(-5),
    left: '50%',
    transform: 'translateX(-50%)',
  },
  table: {
    '& td, & th': {
      padding: theme.spacing(1), // Half of the default
    },
    '& th:first-child, & td:first-child > div': {
      textAlign: 'center',
      justifyContent: 'center',
    },
  },
  tableHeadRow: {
    '& > th': {
      fontWeight: 'normal',
      textTransform: 'uppercase',
      color: theme.palette.foreground.grey4,
    },
  },
  tableRow: {
    '& > td > a': {
      justifyContent: 'flex-start',
    },
  },
}), { name: 'ClustersTable' });

export const ClustersTableInner = React.memo(() => {
  const classes = useStyles();
  const { data, loading, error } = useQuery<{
    clusters: ClusterRowInfo[]
  }>(
    gql`
      query listClusterForAdminPage {
        clusters {
          id
          clusterName
          prettyClusterName
          vizierVersion
          status
          lastHeartbeatMs
          numNodes
          numInstrumentedNodes
        }
      }
    `,
    // Ignore cache on first fetch, to avoid blinking stale heartbeats.
    { pollInterval: 60000, fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );

  const [clusters, setClusters] = React.useState<ClusterRowInfo[]>([]);
  React.useEffect(() => {
    if (data?.clusters) {
      setClusters(data?.clusters
        .filter((cluster) => cluster.lastHeartbeatMs < INACTIVE_CLUSTER_THRESHOLD_MS)
        .sort((clusterA, clusterB) => clusterA.prettyClusterName.localeCompare(clusterB.prettyClusterName)));
    }
  }, [data?.clusters]);

  if (error) {
    return <div className={classes.error}><span>{error.toString()}</span></div>;
  }
  if (!clusters.length) {
    return <div className={classes.error}><span>{ loading ? 'Loading...' : 'No clusters found.' }</span></div>;
  }

  return (
    <div className={classes.root}>
      {loading && <div className={classes.loadingIndicator}><Spinner /></div>}
      <Table className={classes.table}>
        <TableHead>
          <TableRow className={classes.tableHeadRow}>
            <StyledTableHeaderCell>Status</StyledTableHeaderCell>
            <StyledTableHeaderCell>Name</StyledTableHeaderCell>
            <StyledTableHeaderCell>ID</StyledTableHeaderCell>
            <StyledTableHeaderCell>Instrumented Nodes</StyledTableHeaderCell>
            <StyledTableHeaderCell>Vizier Version</StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {clusters.map((cluster: ClusterRowInfo) => (
            <TableRow key={cluster.id} className={classes.tableRow}>
              <ClusterStatusCell status={cluster.status} />
              <StyledTableCell>
                <Button
                  className={classes.removePadding}
                  component={Link}
                  to={getClusterDetailsURL(encodeURIComponent(cluster.clusterName))}
                  color='info'
                  variant='text'
                  disabled={cluster.status === 'CS_DISCONNECTED'}
                >
                  {cluster.prettyClusterName}
                </Button>
              </StyledTableCell>
              <MonoSpaceCell data={cluster.id} />
              <InstrumentationLevelCell cluster={cluster} />
              <VizierVersionCell version={cluster.vizierVersion} />
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </div>
  );
});
ClustersTableInner.displayName = 'ClustersTableInner';

const ClusterDetailsModal = React.memo<RouteComponentProps<{ name: string }>>(({
  history,
  match: { params: { name } },
}) => {
  const classes = useStyles();

  const onClose = React.useCallback(() => {
    history.push('/admin/clusters');
  }, [history]);

  if (!name) return null;
  return (
    <Modal open onClose={onClose}>
      <Card className={classes.modalRoot} elevation={1}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        <ClusterDetails name={name} headerAffix={(
          <Tooltip title='Close (Esc)'>
            <IconButton onClick={onClose}>
              <CloseIcon />
            </IconButton>
          </Tooltip>
        )} />
      </Card>
    </Modal>
  );
});
ClusterDetailsModal.displayName = 'ClusterDetailsModal';


export const ClustersTable = React.memo(() => {
  const { path } = useRouteMatch();
  return (
    <>
      <ClustersTableInner />
      <Switch>
        <Route exact path={`${path}/:name`} component={ClusterDetailsModal} />
        <Route path='*' />
      </Switch>
    </>
  );
});
ClustersTable.displayName = 'ClustersTable';
