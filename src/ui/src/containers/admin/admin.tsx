import { scrollbarStyles } from 'common/mui-theme';
import ProfileMenu from 'containers/live/profile-menu';

import { useQuery } from '@apollo/react-hooks';
import Tab from '@material-ui/core/Tab';
import Tabs from '@material-ui/core/Tabs';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableCell from '@material-ui/core/TableCell';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import Tooltip from '@material-ui/core/Tooltip';
import HealthyIcon from '@material-ui/icons/CheckCircle';
import UnhealthyIcon from '@material-ui/icons/Error';
import UnknownIcon from '@material-ui/icons/Brightness1';
import gql from 'graphql-tag';
import * as React from 'react';
import { Link } from 'react-router-dom';

import { createStyles, makeStyles, Theme, withStyles } from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    root: {
      height: '100%',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: theme.palette.background.default,
      color: theme.palette.text.primary,
      ...scrollbarStyles(theme),
    },
    topBar: {
      display: 'flex',
      margin: theme.spacing(1),
      alignItems: 'center',
    },
    title: {
      ...theme.typography.h6,
      flexGrow: 1,
      fontWeight: theme.typography.fontWeightBold,
      marginLeft: theme.spacing(2),
    },
    main: {
      flex: 1,
      minHeight: 0,
      borderTopStyle: 'solid',
      borderTopColor: theme.palette.background.three,
      borderTopWidth: theme.spacing(0.25),
    },
    link: {
      ...theme.typography.subtitle1,
      margin: theme.spacing(1),
    },
    table: {},
    unhealthy: {
      color: theme.palette.error.main,
    },
    healthy: {
      color: theme.palette.success.main,
    },
    unknown: {
      color: theme.palette.foreground.grey1,
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

function convertHeartbeatMS(lastHeartbeatMs: number): string {
  const secMs = 1000;
  const minMs = 60*secMs;
  const hoursMs = 60*minMs;
  const result = [];
  let time = lastHeartbeatMs;
  const hours = Math.floor(time / (hoursMs));
  if (hours > 0) {
    result.push(`${hours} hours`);
    time = time % hoursMs;
  }
  const minutes = Math.floor(time / minMs);
  if (minutes > 0) {
    result.push(`${minutes} min`);
    time = time % minMs;
  }
  const seconds = Math.floor(time / secMs);
  result.push(`${seconds} sec`);
  return `${result.join(' ')} ago`;
}


type VizierStatusGroup = 'healthy' | 'unhealthy' | 'unknown';
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

function getStatusGroup(status: string): VizierStatusGroup {
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
    statusGroup: getStatusGroup(clusterInfo.status),
    mode: clusterInfo.vizierConfig.passthroughEnabled ? 'Passthrough' : 'Direct',
    lastHeartbeat: convertHeartbeatMS(clusterInfo.lastHeartbeatMs),
  }
}

const StatusCell = ({statusGroup}) => {
  const classes = useStyles();
  switch (statusGroup) {
    case 'healthy':
      return (<HealthyIcon fontSize='small' className={classes.healthy}/>);
    case 'unhealthy':
      return (<UnhealthyIcon fontSize='small' className={classes.unhealthy}/>);
    default:
      return (<UnknownIcon fontSize='small' className={classes.unknown}/>);
  }
}

const ClustersTable = () => {
  const classes = useStyles();

  const { loading, error, data } = useQuery(GET_CLUSTERS, { fetchPolicy: 'network-only' });
  if (loading || error || !data.clusters) {
    return null;
  }
  const clusters = data.clusters.map((cluster) => formatCluster(cluster));
  return (
    <Table className={classes.table}>
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
              <TableCell>{cluster.idShort}</TableCell>
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

const StyledTabs = withStyles((theme: Theme) =>
  createStyles({
    root: {
      flex: 1,
    },
    indicator: {
      backgroundColor: theme.palette.foreground.one,
    },
  }),
)(Tabs);

const StyledTab = withStyles((theme: Theme) =>
  createStyles({
    root: {
      textTransform: 'none',
      '&:focus': {
        color: theme.palette.foreground.two,
      },
    },
  }),
)(Tab);

export default function AdminView() {
  const classes = useStyles();
  const [tab, setTab] = React.useState('clusters');

  return (
    <div className={classes.root}>
      <div className={classes.topBar}>
        <div className={classes.title}>Admin View</div>
        <Link className={classes.link} to='/live'>Live View</Link>
        <ProfileMenu/>
      </div>
      <div className={classes.main}>
        <StyledTabs
          value={tab}
          onChange={(event, newTab) => setTab(newTab)}
        >
          <StyledTab value='clusters' label='Clusters' />
        </StyledTabs>
        {tab == 'clusters' && <ClustersTable/>}
      </div>
    </div>
  );
}
