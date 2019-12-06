import './vizier.scss';

import {vizierGQLClient} from 'common/vizier-gql-client';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {Editor} from 'containers/editor';
import gql from 'graphql-tag';
// @ts-ignore : TS does not like image files.
import * as loadingSvg from 'images/icons/loading-dark.svg';
import * as React from 'react';
import {ApolloConsumer, Query} from 'react-apollo';
import {Route, Switch} from 'react-router-dom';

import {AgentDisplay} from './agent-display';
import {DeployInstructions} from './deploy-instructions';
import {QueryManager} from './query-manager';
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

export interface RouterInfo {
  pathname: string;
}

interface VizierState {
  creatingCluster: boolean;
}

interface VizierProps {
  location: RouterInfo;
}

interface ClusterInstructionsProps {
  message: string;
}

interface VizierMainProps {
  pathname: string;
}

const ClusterInstructions = (props: ClusterInstructionsProps) => (
  <div className='cluster-instructions'>
    <DialogBox width={760}>
      <div className='cluster-instructions--content'>
        {props.message}
        <p></p>
        <img className='spinner' src={loadingSvg} />
      </div>
    </DialogBox>
  </div>
);

export class VizierMain extends React.Component<VizierMainProps, { loaded: boolean }> {
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
            <>
              <VizierTopNav />
              <Switch>
                <Route path={`/vizier/agents`} component={AgentDisplay} />
                <Route path={'/vizier/script'} component={Editor} />
                <Route path={`/`} component={QueryManager} />
              </Switch>
            </>
          );
        }}
      </Query >
    );
  }
}
export class Vizier extends React.Component<VizierProps, VizierState> {
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
                    <VizierMain
                      pathname={this.props.location.pathname}
                    />
                  );
                } else if (data.cluster.status === 'VZ_ST_UNHEALTHY') {
                  const clusterStarting = 'Cluster found. Waiting for pods and services to become ready...';
                  return <ClusterInstructions message={clusterStarting} />;
                } else {
                  return (
                    <DeployInstructions
                      sitename={window.location.hostname.split('.')[0]}
                      clusterID={data.cluster.id}
                    />
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
export default Vizier;
