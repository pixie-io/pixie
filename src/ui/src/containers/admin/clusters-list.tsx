import {convertHeartbeatMS, StatusCell, VizierStatusGroup} from './utils';

import { useQuery } from '@apollo/react-hooks';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableCell from '@material-ui/core/TableCell';
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
          <TableCell></TableCell>
          <TableCell>ID</TableCell>
          <TableCell>Name</TableCell>
          <TableCell>Vizier</TableCell>
          <TableCell>K8s</TableCell>
          <TableCell>Heartbeat</TableCell>
          <TableCell>Mode</TableCell>
        </TableRow>
      </TableHead>
      <TableBody>
        {clusters.map((cluster: ClusterDisplay) => (
          <TableRow key={cluster.id}>
            <Tooltip title={cluster.status} placement='right'>
              <TableCell>
                <StatusCell statusGroup={cluster.statusGroup}/>
              </TableCell>
            </Tooltip>
            <Tooltip title={cluster.id} placement='right'>
              <TableCell>
                <Link to={`/admin/cluster/${cluster.id}`}>{cluster.idShort}</Link>
              </TableCell>
            </Tooltip>
            <TableCell>{cluster.name}</TableCell>
            <Tooltip title={cluster.vizierVersion} placement='right'>
              <TableCell>{cluster.vizierVersionShort}</TableCell>
            </Tooltip>
            <TableCell>{cluster.clusterVersion}</TableCell>
            <TableCell>{cluster.lastHeartbeat}</TableCell>
            <TableCell>{cluster.mode}</TableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
}
