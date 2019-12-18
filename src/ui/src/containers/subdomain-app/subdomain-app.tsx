import './subdomain-app.scss';

import {getCloudGQLClient} from 'common/cloud-gql-client';
import {VersionInfo} from 'components/version-info/version-info';
import Login from 'containers/login';
import Vizier from 'containers/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Route, Router, Switch} from 'react-router-dom';
import {isProd} from 'utils/env';
import history from 'utils/pl-history';

export class SubdomainApp extends React.Component {
  state = {
    client: null,
  };

  async componentDidMount() {
    const client = await getCloudGQLClient();
    this.setState({ client });
  }

  render() {
    const { client } = this.state;

    return !client ?
      <div>Loading...</div> :
      (
        <>
          <Router history={history}>
            <ApolloProvider client={client}>
              <div style={{
                height: '100vh',
                width: '100vw',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'auto',
              }}>
                <Switch>
                  <Route path='/login' component={Login} />
                  <Route path='/create-site' component={Login} />
                  <Route path='/logout' component={Login} />
                  <Route component={Vizier} />
                </Switch>
              </div>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </>
      );
  }
}
