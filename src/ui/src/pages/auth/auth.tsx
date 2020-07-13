// This file has the auth pages that we exposes to the users to login, signup
// and related error pages.
import * as React from 'react';
import history from 'utils/pl-history';
import { Route, Router, Switch } from 'react-router';
import { LoginPage } from './login';
import { SignupPage } from './signup';
import { AuthCallbackPage } from './callback';
import { LogoutPage } from './logout';
import { CLIAuthCompletePage } from './cli-auth-complete';

export const AuthRouter = () => (
  <Router history={history}>
    <Switch>
      <Route exact path='/auth/callback' component={AuthCallbackPage} />
      <Route exact path='/auth/login' component={LoginPage} />
      <Route exact path='/auth/signup' component={SignupPage} />
      <Route exact path='/auth/cli-auth-complete' component={CLIAuthCompletePage} />
      <Route exact path='/auth/logout' component={LogoutPage} />
    </Switch>
  </Router>
);
