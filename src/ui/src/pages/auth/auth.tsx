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

import { Route, Switch } from 'react-router';

import { AuthCallbackPage, CLITokenPage } from './callback';
import { CLIAuthCompletePage } from './cli-auth-complete';
import { InvitePage } from './invite';
import { LoginPage } from './login';
import { LogoutPage } from './logout';
import { ErrorPage } from './password-error';
import { PasswordLoginPage } from './password-login';
import { PasswordRecoveryPage } from './password-recovery';
import { SignupPage } from './signup';
import { SignupCompletePage } from './signup-complete';

// eslint-disable-next-line react-memo/require-memo
export const AuthRouter: React.FC = () => (
  <Switch>
    <Route exact path='/auth/password-login' component={PasswordLoginPage} />
    <Route exact path='/auth/password/recovery' component={PasswordRecoveryPage} />
    <Route exact path='/auth/password/error' component={ErrorPage} />
    <Route exact path='/auth/callback' component={AuthCallbackPage} />
    <Route exact path='/auth/cli-token' component={CLITokenPage} />
    <Route exact path='/auth/login' component={LoginPage} />
    <Route exact path='/auth/invite' component={InvitePage} />
    <Route exact path='/auth/signup' component={SignupPage} />
    <Route exact path='/auth/signup-complete' component={SignupCompletePage} />
    <Route exact path='/auth/cli-auth-complete' component={CLIAuthCompletePage} />
    <Route exact path='/auth/logout' component={LogoutPage} />
  </Switch>
);
AuthRouter.displayName = 'AuthRouter';
