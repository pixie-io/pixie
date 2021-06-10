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

import { FormStructure } from 'app/components';
import { Auth0Buttons } from 'app/containers/auth/auth0-buttons';
import { UserManager } from 'oidc-client';
import { AUTH_CLIENT_ID, AUTH_URI } from 'app/containers/constants';
import { OAuthProviderClient, Token } from './oauth-provider';

// Connection type is the Auth0 Connection type that's currently allowed. Add connection
// values here as needed.
type Connection = 'google-oauth2';
export class Auth0Client extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  // eslint-disable-next-line class-methods-use-this
  makeAuth0OIDCClient(connectionName: Connection, redirectURI: string): UserManager {
    return new UserManager({
      authority: `https://${AUTH_URI}`,
      client_id: AUTH_CLIENT_ID,
      redirect_uri: redirectURI,
      extraQueryParams: {
        connection: connectionName,
      },
      prompt: 'login',
      scope: 'openid profile email',
      // "token" is returned and propagated as the main authorization access_token.
      // "id_token" used by oidc-client-js to verify claims, errors if missing.
      // complaining about a mismatch between repsonse claims and ID token claims.
      response_type: 'token id_token',
    });
  }

  redirectToGoogleLogin(): void {
    this.makeAuth0OIDCClient(
      'google-oauth2',
      this.getRedirectURL(/* isSignup */ false),
    ).signinRedirect();
  }

  redirectToGoogleSignup(): void {
    this.makeAuth0OIDCClient(
      'google-oauth2',
      this.getRedirectURL(/* isSignup */ true),
    ).signinRedirect();
  }

  // eslint-disable-next-line class-methods-use-this
  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      // The callback doesn't require any settings to be created.
      // That means this implementation is agnostic to the OIDC that we connected to.
      new UserManager({}).signinRedirectCallback()
        .then((user) => {
          if (!user) {
            reject(new Error('user is undefined, please try logging in again'));
          }
          resolve({
            accessToken: user.access_token,
            idToken: user.id_token,
            isEmailUnverified: false,
          });
        }).catch(reject);
    });
  }

  // eslint-disable-next-line class-methods-use-this
  async getPasswordLoginFlow(): Promise<FormStructure> {
    throw new Error('Password flow not available for OIDC. Use the proper OIDC flow.');
  }

  // eslint-disable-next-line class-methods-use-this
  async getResetPasswordFlow(): Promise<FormStructure> {
    throw new Error('Reset Password flow not available for OIDC. Use the proper OIDC flow.');
  }

  getLoginButtons(): React.ReactElement {
    return Auth0Buttons({
      googleButtonText: 'Login with Google',
      onGoogleButtonClick: () => this.redirectToGoogleLogin(),
    });
  }

  getSignupButtons(): React.ReactElement {
    return Auth0Buttons({
      googleButtonText: 'Sign-up with Google',
      onGoogleButtonClick: () => this.redirectToGoogleSignup(),
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
