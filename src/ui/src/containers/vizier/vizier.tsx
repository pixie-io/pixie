import './vizier.scss';

import ClusterContext from 'common/cluster-context';
import * as storage from 'common/storage';
import { ClusterStatus, VizierGRPCClientProvider } from 'common/vizier-grpc-client-context';
import { useSnackbar } from 'components/snackbar/snackbar';
import AdminView from 'containers/admin/admin';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import { Editor } from 'containers/editor';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';

import { useQuery } from '@apollo/react-hooks';
import { createStyles, makeStyles } from '@material-ui/core/styles';

import { AgentDisplay } from './agent-display';
import { ClusterInstructions, DeployInstructions } from './deploy-instructions';
import { VizierTopNav } from './top-nav';

export const LIST_CLUSTERS = gql`
{
  clusters {
    id
    status
    lastHeartbeatMs
    vizierVersion
    vizierConfig {
      passthroughEnabled
    }
  }
}`;

export const CHECK_VIZIER = gql`
{
  vizier {
    agents {
      state
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

export const VizierMain = () => {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', width: '100%' }}>
      <VizierTopNav />
      <Switch>
        <Route path='/agents' component={AgentDisplay} />
        <Route path='/console' component={Editor} />
      </Switch>
    </div>
  );
};

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

const Vizier = () => {
  const showSnackbar = useSnackbar();

  const [clusterId, setClusterId] = storage.useSessionStorage(storage.CLUSTER_ID_KEY, '');
  const { loading, error, data } = useQuery(LIST_CLUSTERS, { pollInterval: 2500, fetchPolicy: 'network-only' });

  const context = React.useMemo(() => {
    return {
      selectedCluster: clusterId,
      setCluster: setClusterId,
    };
  }, [clusterId, setClusterId]);

  if (loading) { return <div>Loading...</div>; }

  const errMsg = error?.message;
  if (errMsg) {
    // This is an error with pixie cloud, it is probably not relevant to the user.
    // Show a generic error message instead.
    showSnackbar({ message: 'There was a problem connecting to Pixie', autoHideDuration: 5000 });
    console.error(errMsg);
  }

  if (data.clusters.length === 0) {
    return <DeployInstructions />;
  }

  const cluster = (clusterId && data.clusters.find((c) => c.id === clusterId)) || data.clusters[0];
  const status: ClusterStatus = cluster.status || 'CS_UNKNOWN';

  if (clusterId !== cluster.id) {
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
          <Switch>
            <Route path='/live' component={LiveView} />
            <Route path={['/console', '/agents']} component={VizierMain} />
            <Redirect from='/*' to='/live' />
          </Switch>
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
        <Route component={Vizier} />
      </Switch>
    </>
  );
}
