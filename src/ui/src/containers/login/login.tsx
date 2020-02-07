import './login.scss';

import {getCloudGQLClientSync} from 'common/cloud-gql-client';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Route, Router, Switch} from 'react-router-dom';
import history from 'utils/pl-history';

import {Logout} from './logout';
import {UserCreate, UserLogin} from './user-login';

export const Login = () => (
  <div className='pixie-login center-content'>
    <Router history={history}>
      <ApolloProvider client={getCloudGQLClientSync()}>
        <Switch>
          <Route exact path='/login' component={UserLogin} />
          <Route exact path='/create-site' component={UserCreate} />
          <Route exact path='/logout' component={Logout} />
        </Switch>
      </ApolloProvider>
    </Router>
  </div>
);
