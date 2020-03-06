import './vizier.scss';

import {vizierGQLClient} from 'common/vizier-gql-client';
import {VizierGRPCClientProvider} from 'common/vizier-grpc-client-context';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {Spinner} from 'components/spinner/spinner';
import {Editor} from 'containers/editor';
import LiveView from 'containers/live/live';
import gql from 'graphql-tag';
import * as React from 'react';
import {ApolloConsumer, Query, withApollo} from 'react-apollo';
import {Redirect, Route, Switch} from 'react-router-dom';

import {AgentDisplay} from './agent-display';
import {DeployInstructions} from './deploy-instructions';
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

const PATH_TO_HEADER_TITLE = {
  '/vizier/agents': 'Agents',
  '/vizier/query': 'Query',
};

interface VizierState {
  creatingCluster: boolean;
}

interface ClusterInstructionsProps {
  message: string;
}

const ClusterInstructions = (props: ClusterInstructionsProps) => (
  <div className='cluster-instructions'>
    <DialogBox width={760}>
      <div className='cluster-instructions--content'>
        {props.message}
        <p></p>
        <Spinner variant='dark' />
      </div>
    </DialogBox>
  </div>
);

/**
 * This is need to get the apollo client to the editor, because ApolloProvider
 * in SubdomainApp is provided by the 'react-apollo' package and it is
 * incompatible with the hooks.
 *  TODO(malthus): Refactor apollo usage to all use hooks.
 */
const EditorWithApollo = withApollo(Editor);
const LiveViewWithApollo = withApollo(LiveView);

export class VizierMain extends React.Component<{}, { loaded: boolean }> {
  constructor(props) {
    super(props);
    this.state = { loaded: false };
  }

  render() {
    return (
      <Query client={vizierGQLClient} query={CHECK_VIZIER} pollInterval={2500}>
        {({ loading, error }) => {
          const loaded = this.state.loaded || (!loading && !error) || (error && !error.networkError);
          if (!loaded) {
            // TODO(michelle): Make a separate HTTP request to Vizier so we can get a better error message
            // for Vizier's status.
            const dnsMsg = 'Setting up DNS records for cluster...';
            return <ClusterInstructions message={dnsMsg} />;
          }
          if (!this.state.loaded) {
            this.setState({ loaded });
          }

          return (
            <div style={{ display: 'flex', flexDirection: 'column', height: '100%', width: '100%'}}>
              <VizierTopNav />
              <Switch>
                <Route path='/agents' component={AgentDisplay} />
                <Route path='/console' component={EditorWithApollo} />
                <Redirect from='/*' to='/console' />
              </Switch>
            </div>
          );
        }}
      </Query >
    );
  }
}

export class Vizier extends React.Component<{}, VizierState> {
  constructor(props) {
    super(props);

    this.state = {
      creatingCluster: false,
    };
  }

  render() {
    return (
      <ApolloConsumer>{(client) => {
        return (
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

                if (data.cluster.status === 'VZ_ST_HEALTHY') {
                  return (
                    <VizierGRPCClientProvider>
                      <Switch>
                        <Route path='/live' component={LiveViewWithApollo} />
                        <Route component={VizierMain} />
                      </Switch>
                    </VizierGRPCClientProvider>
                  );
                } else if (data.cluster.status === 'VZ_ST_UNHEALTHY') {
                  const clusterStarting = 'Cluster found. Waiting for pods and services to become ready...';
                  return <ClusterInstructions message={clusterStarting} />;
                } else {
                  return (
                    <DeployInstructions/>
                  );
                }
              }
            }
          </Query>
        );
      }}
      </ApolloConsumer>
    );
  }
}
