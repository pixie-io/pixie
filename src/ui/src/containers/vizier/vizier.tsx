import './vizier.scss';

import {
    CloudClientInterface, VizierGQLClient, VizierGQLClientContext,
} from 'common/vizier-gql-client';
import {VizierGRPCClientProvider} from 'common/vizier-grpc-client-context';
import {CloudClientContext} from 'containers/App/context';
import {Editor} from 'containers/editor';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import {ApolloConsumer, Query, withApollo} from 'react-apollo';
import {Redirect, Route, Switch} from 'react-router-dom';

import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {AgentDisplay} from './agent-display';
import {ClusterInstructions, DeployInstructions} from './deploy-instructions';
import {VizierTopNav} from './top-nav';

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

const PATH_TO_HEADER_TITLE = {
  '/vizier/agents': 'Agents',
  '/vizier/query': 'Query',
};

const useStyles = makeStyles((theme: Theme) => {
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

/**
 * This is need to get the apollo client to the editor, because ApolloProvider
 * in SubdomainApp is provided by the 'react-apollo' package and it is
 * incompatible with the hooks.
 *  TODO(malthus): Refactor apollo usage to all use hooks.
 */
const EditorWithApollo = withApollo(Editor);
const LiveViewWithApollo = withApollo(LiveView);

interface VizierMainProps {
  cloudClient: CloudClientInterface;
}

export const VizierMain = (props: VizierMainProps) => {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', width: '100%' }}>
      <VizierTopNav />
      <Switch>
        <Route path='/admin' component={AgentDisplay} />
        <Route path='/console' component={EditorWithApollo} />
      </Switch>
    </div>
  );
};

interface VizierState {
  creatingCluster: boolean;
}

const ClusterBanner = () => {
  const classes = useStyles();
  return (<Query query={GET_USER} fetchPolicy={'network-only'}>
    {
      ({ loading, error, data }) => {
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
      }
    }
  </Query>);
};

export class Vizier extends React.Component<{}, VizierState> {
  constructor(props) {
    super(props);

    this.state = {
      creatingCluster: false,
    };
  }

  render() {
    return (
      <CloudClientContext.Consumer>
        {(cloudClient) => (
          <ApolloConsumer>{(client) => {
            return (
              <>
                <ClusterBanner />
                <Query query={GET_CLUSTER} pollInterval={2500}>
                  {
                    ({ loading, error, data }) => {
                      if (loading) { return 'Loading...'; }
                      if (error) {
                        // TODO(michelle): Figure out how to add status codes to GQL errors.
                        if (error.message.includes('no clusters')) {
                          // If no cluster exists, and is not already being created, create it.
                          if (!this.state.creatingCluster) {
                            this.setState({ creatingCluster: true });
                            client.mutate({
                              mutation: CREATE_CLUSTER,
                            });
                          }

                          return <ClusterInstructions message='Initializing...' />;
                        }
                        return `Error! ${error.message}`;
                      }
                      if (data.cluster.status === 'CS_HEALTHY') {
                        return (
                          <VizierGRPCClientProvider
                            cloudClient={cloudClient}
                            clusterID={data.cluster.id}
                            passthroughEnabled={data.cluster.vizierConfig.passthroughEnabled}
                            loadingScreen={<ClusterInstructions message='Connecting to cluster...' />}
                          >
                            <Switch>
                              <Route path='/live' component={LiveViewWithApollo} />
                              <Route
                                path={['/console', '/admin']}
                                render={(props) => <VizierMain {...props} cloudClient={cloudClient} />}
                              />
                              <Redirect from='/*' to='/live' />
                            </Switch>
                          </VizierGRPCClientProvider>
                        );
                      } else if (data.cluster.status === 'CS_UNHEALTHY') {
                        const clusterStarting = 'Cluster found. Waiting for pods and services to become ready...';
                        return <ClusterInstructions message={clusterStarting} />;
                      } else {
                        return (
                          <DeployInstructions />
                        );
                      }
                    }
                  }
                </Query>
              </>
            );
          }}
          </ApolloConsumer>
        )}
      </CloudClientContext.Consumer>
    );
  }
}
