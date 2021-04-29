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

import type * as React from 'react';

import auth0 from 'auth0-js';
import {
  AUTH_CLIENT_ID, AUTH_URI,
} from 'containers/constants';
import { FormStructure } from '@pixie-labs/components';
import { Auth0Buttons } from 'containers/auth/auth0-buttons';
import { OAuthProviderClient, Token } from './oauth-provider';

function makeAuth0Client(): auth0.WebAuth {
  return new auth0.WebAuth({
    domain: AUTH_URI,
    clientID: AUTH_CLIENT_ID,
  });
}

export class Auth0Client extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  loginRequest(): void {
    makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ false),
      prompt: 'login',
    });
  }

  signupRequest(): void {
    makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ true),
      prompt: 'login',
    });
  }

  // eslint-disable-next-line class-methods-use-this
  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      makeAuth0Client().parseHash({ hash: window.location.hash }, (errStatus, authResult) => {
        if (errStatus) {
          reject(new Error(`${errStatus.error} - ${errStatus.errorDescription}`));
          return;
        }
        resolve(authResult.accessToken);
      });
    });
  }

  // eslint-disable-next-line class-methods-use-this
  async getPasswordLoginFlow(): Promise<FormStructure> {
    throw new Error('Password flow currently unavailable for Auth0');
  }

  // eslint-disable-next-line class-methods-use-this
  async getResetPasswordFlow(): Promise<FormStructure> {
    throw new Error('Reset password flow currently unavailable for Auth0');
  }

  getLoginButtons(): React.ReactElement {
    return Auth0Buttons({
      googleButtonText: 'Login with Google',
      onGoogleButtonClick: () => this.loginRequest(),
    });
  }

  getSignupButtons(): React.ReactElement {
    return Auth0Buttons({
      googleButtonText: 'Sign-up with Google',
      onGoogleButtonClick: () => this.signupRequest(),
    });
  }

  // eslint-disable-next-line class-methods-use-this
  async getError(): Promise<FormStructure> {
    throw new Error('error flow not supported for Auth0');
  }

  // eslint-disable-next-line class-methods-use-this
  isInvitationEnabled(): boolean {
    return false;
  }

  // eslint-disable-next-line class-methods-use-this
  getInvitationComponent(): React.FC {
    return undefined;
  }
}
