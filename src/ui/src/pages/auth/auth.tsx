/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
