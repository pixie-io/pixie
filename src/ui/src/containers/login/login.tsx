import './login.scss';

import * as React from 'react';
import {Redirect, Route, Router, Switch} from 'react-router-dom';
import history from 'utils/pl-history';

import {Logout} from './logout';
import {UserLogin} from './user-login';

export const Login = () => (
  <div className='pixie-login center-content'>
    <Router history={history}>
      <Switch>
        <Route exact path='/login' component={UserLogin} />
        <Route exact path='/logout' component={Logout} />
        <Redirect to='login' />
      </Switch>
    </Router>
  </div>
);
