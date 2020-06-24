import './vizier.scss';

import ClusterContext from 'common/cluster-context';
import * as storage from 'common/storage';
import {
    CLUSTER_STATUS_CONNECTED, CLUSTER_STATUS_DISCONNECTED, CLUSTER_STATUS_HEALTHY,
    CLUSTER_STATUS_UNHEALTHY, CLUSTER_STATUS_UNKNOWN, CLUSTER_STATUS_UPDATE_FAILED,
    CLUSTER_STATUS_UPDATING, ClusterStatus, VizierGRPCClientProvider,
} from 'common/vizier-grpc-client-context';
import { useSnackbar } from 'components/snackbar/snackbar';
import AdminView from 'containers/admin/admin';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';

import { useQuery } from '@apollo/react-hooks';
import { createStyles, makeStyles } from '@material-ui/core/styles';

import { DeployInstructions } from './deploy-instructions';

export const LIST_CLUSTERS = gql`
{
  clusters {
    id
    clusterName
    status
    vizierConfig {
      passthroughEnabled
    }
  }
}`;

const GET_USER = gql`
{
  user {
    email
    orgName
  }
}
`;

const useStyles = makeStyles(() => {
  return createStyles({
    banner: {
      position: 'absolute',
      width: '100%',
      textAlign: 'center',
      top: 0,
      zIndex: 1,
      color: 'white',
      background: 'rgba(220,0,0,0.5)',
    },
  });
});

const ClusterBanner = () => {
  const classes = useStyles();
  const { loading, error, data } = useQuery(GET_USER, { fetchPolicy: 'network-only' });

  if (loading || error) {
    return null;
  }

  if (data.user.email.split('@')[1] === 'pixie.support') {
    return (
      <div className={classes.banner}>
        {
          'You are viewing clusters for an external org: ' + data.user.orgName
        }
      </div>
    );
  }

  return null;
};

interface ClusterInfo {
  id: string;
  clusterName: string;
  status: string;
}

// Selects a default cluster if one hasn't already been selected by the user.
// Selects based on cluster status and tiebreaks by cluster name.
export function selectCluster(clusters: ClusterInfo[]): ClusterInfo {
  if (clusters.length === 0) {
    return null;
  }
  // Buckets cluster states by desirability for selection.
  // 0 = most prioritized.
  const clusterStatusMap = {
    [CLUSTER_STATUS_UNKNOWN]: 3,
    [CLUSTER_STATUS_HEALTHY]: 0,
    [CLUSTER_STATUS_UNHEALTHY]: 2,
    [CLUSTER_STATUS_DISCONNECTED]: 3,
    [CLUSTER_STATUS_UPDATING]: 1,
    [CLUSTER_STATUS_CONNECTED]: 1,
    [CLUSTER_STATUS_UPDATE_FAILED]: 2,
  }
  const defaultStatusValue = 3;

  clusters.sort((cluster1, cluster2) => {
    const status1 = clusterStatusMap[cluster1.status] === undefined ?
      defaultStatusValue : clusterStatusMap[cluster1.status];
    const status2 = clusterStatusMap[cluster2.status] === undefined ?
      defaultStatusValue : clusterStatusMap[cluster2.status];
    if (status1 < status2) {
      return -1;
    }
    if (status1 > status2) {
      return 1;
    }
    return cluster1.clusterName < cluster2.clusterName ? -1 : 1;
  });

  return clusters[0];
}

const Vizier = () => {
  const showSnackbar = useSnackbar();

  const [clusterId, setClusterId] = storage.useSessionStorage(storage.CLUSTER_ID_KEY, '');
  const { loading, error, data } = useQuery(LIST_CLUSTERS, { pollInterval: 2500, fetchPolicy: 'network-only' });
  const clusters = data?.clusters || [];
  const cluster = (clusterId && clusters.find((c) => c.id === clusterId)) || selectCluster(clusters);

  const context = React.useMemo(() => {
    return {
      selectedCluster: clusterId,
      selectedClusterName: cluster?.clusterName,
      setCluster: setClusterId,
      setClusterByName: (name: string) => {
        const newClusterId = name && clusters.find((c) => c.clusterName === name)?.id || clusterId;
        setClusterId(newClusterId);
      },
    };
  }, [clusterId, setClusterId, cluster?.clusterName, clusters.length]);

  if (loading) { return <div>Loading...</div>; }

  const errMsg = error?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    console.error(errMsg);
  }

  if (clusters.length === 0) {
    return <DeployInstructions />;
  }

  const status: ClusterStatus = cluster?.status || 'CS_UNKNOWN';

  if (cluster?.id && clusterId !== cluster?.id) {
    setClusterId(cluster.id);
  }

  return (
    <ClusterContext.Provider value={context}>
      <VizierGRPCClientProvider
        clusterID={cluster.id}
        passthroughEnabled={cluster.vizierConfig.passthroughEnabled}
        clusterStatus={errMsg ? 'CS_UNKNOWN' : status}
      >
        <ScriptsContextProvider>
          <LiveView />
        </ScriptsContextProvider>
      </VizierGRPCClientProvider>
    </ClusterContext.Provider>
  );
};

export default function withClusterBanner() {
  return (
    <>
      <ClusterBanner />
      <Switch>
        <Route path='/admin' component={AdminView} />
        <Route path='/live' component={Vizier} />
        <Redirect from='/*' to='/live' />
      </Switch>
    </>
  );
}
