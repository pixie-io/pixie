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
import * as infoImage from 'images/icons/agent.svg';
// @ts-ignore : TS does not like image files.
import * as loadingSvg from 'images/icons/loading-dark.svg';
// @ts-ignore : TS does not like image files.
import * as codeImage from 'images/icons/query.svg';
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
export class Vizier extends React.Component<VizierProps, VizierState> {
  constructor(props) {
    super(props);

    this.state = {
      creatingCluster: false,
    };
  }

  renderClusterInstructions(message: string) {
     return (
      <div className='cluster-instructions'>
        <DialogBox width={760}>
          <div className='cluster-instructions--content'>
            {message}
            <p></p>
            <img className='spinner' src={loadingSvg} />
          </div>
        </DialogBox>
      </div>
    );
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

                  return this.renderClusterInstructions('Initializing...');
                }
                return `Error! ${error.message}`;
              }

              if (data.cluster.status === 'VZ_ST_HEALTHY') {
                return (
                  <div className='vizier'>
                    <SidebarNav
                      logo = {logoImage}
                      items={[
                        { link: '/vizier/query', selectedImg: codeImage, unselectedImg: codeImage },
                        { link: '/vizier/agents', selectedImg: infoImage, unselectedImg: infoImage },
                      ]}
                    />
                    <div className='vizier-body'>
                      <Header
                         primaryHeading='Pixie Console'
                         secondaryHeading={PATH_TO_HEADER_TITLE[this.props.location.pathname]}
                      />
                      <Switch>
                        <Route path={`/vizier/agents`} component={AgentDisplay} />
                        <Route path={`/`} component={QueryManager} />
                      </Switch>
                    </div>
                  </div>
                );
              } else if (data.cluster.status === 'VZ_ST_UNHEALTHY') {
                return this.renderClusterInstructions(
                  'Cluster found. Waiting for pods and services to become ready...');
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
