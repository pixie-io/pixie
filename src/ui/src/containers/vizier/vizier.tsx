import {DialogBox} from 'components/dialog-box/dialog-box';
import {Header} from 'components/header/header';
import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import gql from 'graphql-tag';
import * as React from 'react';
import {ApolloConsumer, Query} from 'react-apollo';
import {Route, Switch} from 'react-router-dom';
import {AgentDisplay} from './agent-display';
import {DeployInstructions} from './deploy-instructions';
import {QueryManager} from './query-manager';

import './vizier.scss';

// TODO(zasgar/michelle): we should figure out a good way to
// package assets.
// @ts-ignore : TS does not like image files.
import * as infoImage from 'images/icons/agent-dark.svg';
// @ts-ignore : TS does not like image files.
import * as docsImage from 'images/icons/docs-light.svg';
// @ts-ignore : TS does not like image files.
import * as loadingSvg from 'images/icons/loading-dark.svg';
// @ts-ignore : TS does not like image files.
import * as codeImage from 'images/icons/query.svg';
// @ts-ignore : TS does not like image files.
import * as userImage from 'images/icons/user.svg';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/logo.svg';

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
  vizier
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

export class VizierMain extends React.Component<VizierMainProps, {}> {
  render() {
    return (
      <Query context={{clientName: 'vizier'}} query={CHECK_VIZIER} pollInterval={2500}>
        {({ loading, error, data }) => {
          if (loading) {
            return <div></div>;
          }

          if (error && error.networkError) {
            // TODO(michelle): Make a separate HTTP request to Vizier so we can get a better error message
            // for Vizier's status.
            const dnsMsg = 'Setting up DNS records for cluster...';
            return <ClusterInstructions message={dnsMsg}/>;
          }

          return (<div className='vizier'>
            <SidebarNav
              logo = {logoImage}
              items={[
                { link: '/vizier/query', selectedImg: codeImage, unselectedImg: codeImage },
              ]}
              footerItems={[
                { link: '/vizier/agents', selectedImg: infoImage, unselectedImg: infoImage },
                { link: '/docs/getting-started/', selectedImg: docsImage, unselectedImg: docsImage },
                { selectedImg: userImage, unselectedImg: userImage, menu: {'Sign out': '/logout'}},
              ]}
            />
            <div className='vizier-body'>
              <Header
                 primaryHeading='Pixie Console'
                 secondaryHeading={PATH_TO_HEADER_TITLE[this.props.pathname]}
              />
              <Switch>
                <Route path={`/vizier/agents`} component={AgentDisplay} />
                <Route path={`/`} component={QueryManager} />
              </Switch>
            </div>
          </div>);
        }}
      </Query>
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
            ({loading, error, data}) => {
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
