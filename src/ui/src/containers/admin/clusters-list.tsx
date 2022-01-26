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
  Button,
  Table,
  TableBody,
  TableHead,
  TableRow,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Link } from 'react-router-dom';

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
  error: {
    padding: theme.spacing(1),
  },
  removePadding: {
    padding: 0,
  },
}), { name: 'ClustersTable' });

export const ClustersTable = React.memo(() => {
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

  const clusters = data?.clusters
    .filter((cluster) => cluster.lastHeartbeatMs < INACTIVE_CLUSTER_THRESHOLD_MS)
    .sort((clusterA, clusterB) => clusterA.prettyClusterName.localeCompare(clusterB.prettyClusterName));

  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }
  if (!clusters) {
    return <div className={classes.error}>No clusters found.</div>;
  }

  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell />
          <StyledTableHeaderCell>Name</StyledTableHeaderCell>
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
          <StyledTableHeaderCell>Instrumented Nodes</StyledTableHeaderCell>
          <StyledTableHeaderCell>Vizier Version</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {clusters.map((cluster: ClusterRowInfo) => (
          <TableRow key={cluster.id}>
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
  );
});
ClustersTable.displayName = 'ClustersTable';
