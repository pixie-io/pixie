import './login.scss';

import * as React from 'react';
import { Route, Router, Switch } from 'react-router-dom';
import history from 'utils/pl-history';

import { Logout } from './logout';
import { UserCreate, UserLogin } from './user-login';

export const Login = () => (
  <div className='pixie-login'>
    <Router history={history}>
      <Switch>
        <Route exact path='/login' component={UserLogin} />
        <Route exact path='/logout' component={Logout} />
        <Route exact path='/signup' component={UserCreate} />
      </Switch>
    </Router>
  </div>
);
