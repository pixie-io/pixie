import './vizier.scss';

import ClusterContext from 'common/cluster-context';
import * as storage from 'common/storage';
import { ClusterStatus, VizierGRPCClientProvider } from 'common/vizier-grpc-client-context';
import { useSnackbar } from 'components/snackbar/snackbar';
import AdminView from 'pages/admin/admin';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';

import { useQuery } from '@apollo/react-hooks';
import { createStyles, makeStyles } from '@material-ui/core/styles';

import { DeployInstructions } from './deploy-instructions';
import { selectCluster } from './cluster-info';

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

const useStyles = makeStyles(() => createStyles({
  banner: {
    position: 'absolute',
    width: '100%',
    textAlign: 'center',
    top: 0,
    zIndex: 1,
    color: 'white',
    background: 'rgba(220,0,0,0.5)',
  },
}));

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
          `You are viewing clusters for an external org: ${data.user.orgName}`
        }
      </div>
    );
  }

  return null;
};

// Selects a default cluster if one hasn't already been selected by the user.

const Vizier = () => {
  const showSnackbar = useSnackbar();

  const [clusterId, setClusterId] = storage.useSessionStorage(storage.CLUSTER_ID_KEY, '');
  const { loading, error, data } = useQuery(LIST_CLUSTERS, { pollInterval: 2500, fetchPolicy: 'network-only' });
  const clusters = data?.clusters || [];
  const cluster = (clusterId && clusters.find((c) => c.id === clusterId)) || selectCluster(clusters);

  const context = React.useMemo(() => ({
    selectedCluster: clusterId,
    selectedClusterName: cluster?.clusterName,
    setCluster: setClusterId,
    setClusterByName: (name: string) => {
      const newClusterId = name && clusters.find((c) => c.clusterName === name)?.id || clusterId;
      setClusterId(newClusterId);
    },
  }), [clusterId, setClusterId, cluster?.clusterName, clusters.length]);

  if (loading) { return <div>Loading...</div>; }

  const errMsg = error?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    // eslint-disable-next-line no-console
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
