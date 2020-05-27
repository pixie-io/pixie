import './vizier.scss';

import { VizierGRPCClientProvider } from 'common/vizier-grpc-client-context';
import { ScriptsContextProvider } from 'containers/App/scripts-context';
import { Editor } from 'containers/editor';
import AdminView from 'containers/admin/admin';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import { Redirect, Route, Switch } from 'react-router-dom';

import { useApolloClient, useQuery } from '@apollo/react-hooks';
import { createStyles, makeStyles } from '@material-ui/core/styles';

import { AgentDisplay } from './agent-display';
import { ClusterInstructions, DeployInstructions } from './deploy-instructions';
import { VizierTopNav } from './top-nav';

export const CREATE_CLUSTER = gql`
  mutation CreateCluster {
    CreateCluster {
      id
    }
  }
`;

export const GET_CLUSTER = gql`
{
  cluster {
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
  const [creatingCluster, setCreatingCluster] = React.useState(false);
  const client = useApolloClient();

  const { loading, error, data } = useQuery(GET_CLUSTER, { pollInterval: 2500, fetchPolicy: 'network-only' });

  if (loading) { return <div>Loading...</div>; }

  if (error) {
    // TODO(michelle): Figure out how to add status codes to GQL errors.
    if (error.message.includes('no clusters')) {
      // If no cluster exists, and is not already being created, create it.
      if (!creatingCluster) {
        setCreatingCluster(true);
        client.mutate({
          mutation: CREATE_CLUSTER,
        });
      }

      return <ClusterInstructions message='Initializing...' />;
    }
    return <div>Error! {error.message}</div>;
  }

  if (data.cluster.status === 'CS_HEALTHY') {
    return (
      <VizierGRPCClientProvider
        clusterID={data.cluster.id}
        passthroughEnabled={data.cluster.vizierConfig.passthroughEnabled}
        loadingScreen={<ClusterInstructions message='Connecting to cluster...' />}
        vizierVersion={data.cluster.vizierVersion}
      >
        <ScriptsContextProvider>
          <Switch>
            <Route path='/live' component={LiveView} />
            <Route path={['/console', '/agents']} component={VizierMain}
            />
            <Route path='/admin' component={AdminView} />
            <Redirect from='/*' to='/live' />
          </Switch>
        </ScriptsContextProvider>
      </VizierGRPCClientProvider>
    );
  } else if (data.cluster.status === 'CS_UNHEALTHY') {
    const clusterStarting = 'Cluster found. Waiting for pods and services to become ready...';
    return <ClusterInstructions message={clusterStarting} />;
  } else if (data.cluster.status === 'CS_UPDATING') {
    return <ClusterInstructions message='Cluster updating...' />;
  } else {
    return (
      <DeployInstructions />
    );
  }
};

export default function withClusterBanner() {
  return (
    <>
      <ClusterBanner />
      <Vizier />
    </>
  );
}
