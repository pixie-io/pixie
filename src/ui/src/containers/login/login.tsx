import './login.scss';

import {getCloudGQLClientSync} from 'common/cloud-gql-client';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Route, Router, Switch} from 'react-router-dom';
import history from 'utils/pl-history';

import {Logout} from './logout';
import {UserCreate, UserLogin} from './user-login';

export const Login = () => {
  return (<div className='pixie-login'>
      <Router history={history}>
        <ApolloProvider client={getCloudGQLClientSync()}>
          <Switch>
            <Route exact path='/login' component={UserLogin} />
            <Route exact path='/logout' component={Logout} />
            <Route component={UserCreate} />
          </Switch>
        </ApolloProvider>
      </Router>
    </div>
  );
};
