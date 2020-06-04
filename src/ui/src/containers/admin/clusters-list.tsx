import {AdminTooltip, convertHeartbeatMS, StatusCell, StyledTableCell, StyledTableHeaderCell,
        StyledLeftTableCell, StyledRightTableCell, VizierStatusGroup} from './utils';

import { useQuery } from '@apollo/react-hooks';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import Tooltip from '@material-ui/core/Tooltip';
import gql from 'graphql-tag';
import * as React from 'react';
import { Link } from 'react-router-dom';

const GET_CLUSTERS = gql`
{
  clusters {
    id
    clusterName
    clusterVersion
    status
    lastHeartbeatMs
    vizierVersion
    vizierConfig {
      passthroughEnabled
    }
  }
}`;

type VizierConnectionMode = 'Passthrough' | 'Direct';

interface ClusterDisplay {
  id: string;
  idShort: string;
  name: string;
  status: string;
  statusGroup: VizierStatusGroup;
  clusterVersion: string;
  vizierVersionShort: string;
  vizierVersion: string;
  lastHeartbeat: string;
  mode: VizierConnectionMode;
}

function getClusterStatusGroup(status: string): VizierStatusGroup {
  if (['CS_HEALTHY', 'CS_UPDATING', 'CS_CONNECTED'].indexOf(status) != -1) {
    return 'healthy';
  } else if (['CS_UNHEALTHY', 'CS_UPDATE_FAILED'].indexOf(status) != -1) {
    return 'unhealthy';
  } else {
    return 'unknown';
  }
}

export function formatCluster(clusterInfo): ClusterDisplay {
  let shortVersion = clusterInfo.vizierVersion;
  // Dashes occur in internal Vizier versions and not public release ones.
  if (clusterInfo.vizierVersion.indexOf('-') == -1) {
    shortVersion = clusterInfo.vizierVersion.split('+')[0];
  }

  return {
    id: clusterInfo.id,
    idShort: clusterInfo.id.split('-').pop(),
    name: clusterInfo.clusterName,
    clusterVersion: clusterInfo.clusterVersion,
    vizierVersionShort: shortVersion,
    vizierVersion: clusterInfo.vizierVersion,
    status: clusterInfo.status.replace('CS_', ''),
    statusGroup: getClusterStatusGroup(clusterInfo.status),
    mode: clusterInfo.vizierConfig.passthroughEnabled ? 'Passthrough' : 'Direct',
    lastHeartbeat: convertHeartbeatMS(clusterInfo.lastHeartbeatMs),
  }
}

export const ClustersTable = () => {
  const { loading, error, data } = useQuery(GET_CLUSTERS, { fetchPolicy: 'network-only' });
  if (loading || error || !data.clusters) {
    return null;
  }
  const clusters = data.clusters.map((cluster) => formatCluster(cluster));
  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell></StyledTableHeaderCell>
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
          <StyledTableHeaderCell>Name</StyledTableHeaderCell>
          <StyledTableHeaderCell>Vizier</StyledTableHeaderCell>
          <StyledTableHeaderCell>K8s</StyledTableHeaderCell>
          <StyledTableHeaderCell>Heartbeat</StyledTableHeaderCell>
          <StyledTableHeaderCell>Mode</StyledTableHeaderCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {clusters.map((cluster: ClusterDisplay) => (
          <TableRow key={cluster.id}>
            <AdminTooltip title={cluster.status}>
              <StyledLeftTableCell>
                <StatusCell statusGroup={cluster.statusGroup}/>
              </StyledLeftTableCell>
            </AdminTooltip>
            <AdminTooltip title={cluster.id}>
              <StyledTableCell>
                <Link to={`/admin/cluster/${cluster.id}`}>{cluster.idShort}</Link>
              </StyledTableCell>
            </AdminTooltip>
            <StyledTableCell>{cluster.name}</StyledTableCell>
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
}
