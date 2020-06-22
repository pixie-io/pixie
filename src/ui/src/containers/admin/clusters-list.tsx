import { StatusCell, StatusGroup } from 'components/status/status';
import {AdminTooltip, clusterStatusGroup, convertHeartbeatMS, StyledTableCell,
        StyledTableHeaderCell, StyledLeftTableCell, StyledRightTableCell} from './utils';
import { useQuery } from '@apollo/react-hooks';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import Tooltip from '@material-ui/core/Tooltip';
import gql from 'graphql-tag';
import * as React from 'react';
import { Link } from 'react-router-dom';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    error: {
      padding: theme.spacing(1),
    },
  });
});

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
  statusGroup: StatusGroup;
  clusterVersion: string;
  vizierVersionShort: string;
  vizierVersion: string;
  lastHeartbeat: string;
  mode: VizierConnectionMode;
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
    statusGroup: clusterStatusGroup(clusterInfo.status),
    mode: clusterInfo.vizierConfig.passthroughEnabled ? 'Passthrough' : 'Direct',
    lastHeartbeat: convertHeartbeatMS(clusterInfo.lastHeartbeatMs),
  }
}

const CLUSTERS_POLL_INTERVAL = 2500;

export const ClustersTable = () => {
  const classes = useStyles();
  const { loading, error, data } = useQuery(GET_CLUSTERS, { pollInterval: CLUSTERS_POLL_INTERVAL });
  if (loading) {
    return <div className={classes.error}>Loading...</div>;
  }
  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }
  if (!data || !data.clusters) {
    return <div className={classes.error}>No clusters found.</div>;
  }

  const clusters = data.clusters.map((cluster) => formatCluster(cluster));
  return (
    <Table>
      <TableHead>
        <TableRow>
          <StyledTableHeaderCell></StyledTableHeaderCell>
          <StyledTableHeaderCell>Name</StyledTableHeaderCell>
          <StyledTableHeaderCell>ID</StyledTableHeaderCell>
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
            <StyledTableCell>
              <Link to={`/admin/clusters/${encodeURIComponent(cluster.name)}`}>{cluster.name}</Link>
            </StyledTableCell>
            <AdminTooltip title={cluster.id}>
              <StyledTableCell>{cluster.idShort}</StyledTableCell>
            </AdminTooltip>
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
