import './App.scss';

import {getCloudGQLClient} from 'common/cloud-gql-client';
import {VersionInfo} from 'components/version-info/version-info';
import {Login} from 'containers/login';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Route, Router, Switch} from 'react-router-dom';
import {isProd} from 'utils/env';
import history from 'utils/pl-history';

export class App extends React.Component {
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
              <div className='main-page'>
                <div className='content'>
                  <Switch>
                    <Route exact path='/create' component={Login} />
                    <Route exact path='/auth_success' component={Login} />
                    <Route component={Login} />
                  </Switch>
                </div>
              </div>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </>
      );
  }
}
