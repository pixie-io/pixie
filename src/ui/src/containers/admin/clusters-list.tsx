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

import { StatusCell, StatusGroup } from '@pixie-labs/components';
import { Theme, withStyles } from '@material-ui/core/styles';
import Button from '@material-ui/core/Button';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import * as React from 'react';
import { Link } from 'react-router-dom';
import { GaugeLevel } from 'utils/metric-thresholds';
import { useListClusters } from '@pixie-labs/api-react';
import {
  AdminTooltip, clusterStatusGroup, convertHeartbeatMS, getClusterDetailsURL,
  StyledTableCell, StyledTableHeaderCell, StyledLeftTableCell, StyledRightTableCell,
} from './utils';

const INACTIVE_AGENT_THRESHOLD_MS = 24 * 60 * 60 * 1000;

type VizierConnectionMode = 'Passthrough' | 'Direct';

interface ClusterDisplay {
  id: string;
  idShort: string;
  name: string;
  prettyName: string;
  status: string;
  statusGroup: StatusGroup;
  clusterVersion: string;
  vizierVersionShort: string;
  vizierVersion: string;
  lastHeartbeat: string;
  mode: VizierConnectionMode;
  percentInstrumented: string;
  percentInstrumentedLevel: GaugeLevel;
}

function getPercentInstrumentedLevel(instrumentedRatio: number): GaugeLevel {
  if (instrumentedRatio > 0.9) {
    return 'high';
  }
  if (instrumentedRatio > 0.6) {
    return 'med';
  }
  return 'low';
}

function formatCluster(clusterInfo): ClusterDisplay {
  const {
    id, clusterName, prettyClusterName, clusterVersion, vizierVersion, vizierConfig,
    status, lastHeartbeatMs, numNodes, numInstrumentedNodes,
  } = clusterInfo;

  let vizierVersionShort = vizierVersion;
  // Dashes occur in internal Vizier versions and not public release ones.
  if (vizierVersion.indexOf('-') === -1) {
    [vizierVersionShort] = clusterInfo.vizierVersion.split('+');
  }

  let percentInstrumented;
  let percentInstrumentedLevel;
  const trimmedStatus = status.replace('CS_', '');
  if (trimmedStatus !== 'DISCONNECTED') {
    const instrumentedPerc = numNodes ? `${(numInstrumentedNodes / numNodes * 100).toFixed(0)}%` : 'N/A';
    percentInstrumented = `${instrumentedPerc} (${numInstrumentedNodes} of ${numNodes})`;
    percentInstrumentedLevel = numNodes ? getPercentInstrumentedLevel(numInstrumentedNodes / numNodes) : 'low';
  } else {
    percentInstrumented = 'N/A';
    percentInstrumentedLevel = 'none';
  }

  return {
    id,
    clusterVersion,
    vizierVersion,
    vizierVersionShort,
    percentInstrumented,
    percentInstrumentedLevel,
    idShort: id.split('-').pop(),
    name: clusterName,
    prettyName: prettyClusterName,
    status: trimmedStatus,
    statusGroup: clusterStatusGroup(status),
    mode: vizierConfig.passthroughEnabled ? 'Passthrough' : 'Direct',
    lastHeartbeat: convertHeartbeatMS(lastHeartbeatMs),
  };
}

export function formatClusters(clusterInfos): ClusterDisplay[] {
  return clusterInfos
    .filter((cluster) => cluster.lastHeartbeatMs < INACTIVE_AGENT_THRESHOLD_MS)
    .map((cluster) => formatCluster(cluster))
    .sort((clusterA, clusterB) => {
      if (clusterA.prettyName < clusterB.prettyName) {
        return -1;
      }
      if (clusterA.prettyName > clusterB.prettyName) {
        return 1;
      }
      return 0;
    });
}

export const ClustersTable = withStyles((theme: Theme) => ({
  low: {
    color: theme.palette.error.main,
  },
  med: {
    color: theme.palette.warning.main,
  },
  high: {
    color: theme.palette.success.main,
  },
  error: {
    padding: theme.spacing(1),
  },
}))(({ classes }: any) => {
  const [clustersRaw, loading, error] = useListClusters();
  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }
  if (!clustersRaw) {
    return <div className={classes.error}>No clusters found.</div>;
  }

  const clusters = formatClusters(clustersRaw);

  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell />
          <StyledTableHeaderCell>Name</StyledTableHeaderCell>
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
          <StyledTableHeaderCell>Instrumented Nodes</StyledTableHeaderCell>
          <StyledTableHeaderCell>Vizier Version</StyledTableHeaderCell>
          <StyledTableHeaderCell>K8s Version</StyledTableHeaderCell>
          <StyledTableHeaderCell>Heartbeat</StyledTableHeaderCell>
          <StyledTableHeaderCell>Mode</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {clusters.map((cluster: ClusterDisplay) => (
          <TableRow key={cluster.id}>
            <AdminTooltip title={cluster.status}>
              <StyledLeftTableCell>
                <StatusCell statusGroup={cluster.statusGroup} />
              </StyledLeftTableCell>
            </AdminTooltip>
            <AdminTooltip title={cluster.name}>
              <StyledTableCell>
                <Button
                  component={Link}
                  to={getClusterDetailsURL(encodeURIComponent(cluster.name))}
                  color='secondary'
                  variant='text'
                  disabled={cluster.status === 'DISCONNECTED'}
                >
                  {cluster.prettyName}
                </Button>
              </StyledTableCell>
            </AdminTooltip>
            <AdminTooltip title={cluster.id}>
              <StyledTableCell>{cluster.idShort}</StyledTableCell>
            </AdminTooltip>
            <StyledTableCell className={classes[cluster.percentInstrumentedLevel]}>
              {cluster.percentInstrumented}
            </StyledTableCell>
            <AdminTooltip title={cluster.vizierVersion}>
              <StyledTableCell>{cluster.vizierVersionShort}</StyledTableCell>
            </AdminTooltip>
            <StyledTableCell>{cluster.clusterVersion}</StyledTableCell>
            <StyledTableCell>{cluster.lastHeartbeat}</StyledTableCell>
            <StyledRightTableCell>{cluster.mode}</StyledRightTableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
});
