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

import { UserManager } from 'oidc-client';

import { FormStructure } from 'app/components';
import { OIDCButtons } from 'app/containers/auth/oidc-buttons';
import { AUTH_CLIENT_ID, AUTH_URI } from 'app/containers/constants';

import { OAuthProviderClient, Token } from './oauth-provider';

export class OIDCClient extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  /* eslint-disable class-methods-use-this */
  makeOIDCClient(redirectURI: string): UserManager {
    return new UserManager({
      authority: `https://${AUTH_URI}`,
      client_id: AUTH_CLIENT_ID,
      redirect_uri: redirectURI,
      prompt: 'login',
      scope: 'openid profile email',
      response_type: 'token id_token',
    });
  }

  redirectToLogin(): void {
    this.makeOIDCClient(this.getRedirectURL(/* isSignup */ false)).signinRedirect();
  }

  redirectToSignup(): void {
    this.makeOIDCClient(this.getRedirectURL(/* isSignup */ true)).signinRedirect();
  }

  refetchToken(): void {
    const client = new UserManager({
      authority: `https://${AUTH_URI}`,
      client_id: AUTH_CLIENT_ID,
      redirect_uri: this.getRedirectURL(/* isSignup */ false),
      scope: 'openid profile email',
      response_type: 'token id_token',
    });
    client.signinRedirect();
  }

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
          });
        }).catch(reject);
    });
  }

  async getPasswordLoginFlow(): Promise<FormStructure> {
    throw new Error('Password flow not available for OIDC. Use the proper OIDC flow.');
  }

  async getResetPasswordFlow(): Promise<FormStructure> {
    throw new Error('Reset Password flow not available for OIDC. Use the proper OIDC flow.');
  }

  getLoginButtons(): React.ReactElement {
    return OIDCButtons({
      loginButtonText: 'Login',
      onLoginButtonClick: () => this.redirectToLogin(),
    });
  }

  getSignupButtons(): React.ReactElement {
    return OIDCButtons({
      loginButtonText: 'Sign-up',
      onLoginButtonClick: () => this.redirectToSignup(),
    });
  }

  async getError(): Promise<FormStructure> {
    throw new Error('error flow not supported for OIDC');
  }

  isInvitationEnabled(): boolean {
    return false;
  }

  getInvitationComponent(): React.FC {
    return undefined;
  }
  /* eslint-enable class-methods-use-this */
}
