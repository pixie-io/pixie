import './App.scss';

import {getCloudGQLClient} from 'common/cloud-gql-client';
import {VersionInfo} from 'components/version-info/version-info';
import Login from 'containers/login';
import {AuthComplete} from 'containers/login/auth-complete';
import {CompanyCreate, CompanyLogin} from 'containers/login/company-login';
import {UserCreate} from 'containers/login/user-login';
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
              <div className='pixie-main-app center-content'>
                <Switch>
                  <Route exact path='/create' component={CompanyCreate} />
                  <Route exact path='/create-site' component={UserCreate} />
                  <Route exact path='/auth-complete' component={AuthComplete} />
                  <Route component={CompanyLogin} />
                </Switch>
              </div>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </>
      );
  }
}
