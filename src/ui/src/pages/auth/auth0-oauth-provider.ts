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

import { FormStructure } from 'app/components';
import { Auth0Buttons } from 'app/containers/auth/auth0-buttons';
import { AUTH_CLIENT_ID, AUTH_EMAIL_PASSWORD_CONN, AUTH_URI } from 'app/containers/constants';
import { UserManager } from 'oidc-client';
import type * as React from 'react';
import { OAuthProviderClient, Token } from './oauth-provider';

export class Auth0Client extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  // eslint-disable-next-line class-methods-use-this
  makeAuth0OIDCClient(redirectURI: string, extraQueryParams?: Record<string, any>): UserManager {
    return new UserManager({
      authority: `https://${AUTH_URI}`,
      client_id: AUTH_CLIENT_ID,
      redirect_uri: redirectURI,
      extraQueryParams,
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
      this.getRedirectURL(/* isSignup */ false),
      {
        connection: 'google-oauth2',
      },
    ).signinRedirect();
  }

  redirectToGoogleSignup(): void {
    this.makeAuth0OIDCClient(
      this.getRedirectURL(/* isSignup */ true),
      {
        connection: 'google-oauth2',
      },
    ).signinRedirect();
  }

  redirectToEmailLogin(): void {
    this.makeAuth0OIDCClient(
      this.getRedirectURL(/* isSignup */ false),
      {
        connection: AUTH_EMAIL_PASSWORD_CONN,
        // Manually configured in Classic Universal Login settings.
        mode: 'login',
      },
    ).signinRedirect();
  }

  redirectToEmailSignup(): void {
    this.makeAuth0OIDCClient(
      this.getRedirectURL(/* isSignup */ true),
      {
        connection: AUTH_EMAIL_PASSWORD_CONN,
        // Manually configured in Classic Universal Login settings.
        mode: 'signUp',
        // Used by New Universal Login https://auth0.com/docs/login/universal-login/new-experience#signup
        screen_hint: 'signup',
      },
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
      enableEmailPassword: !!AUTH_EMAIL_PASSWORD_CONN,
      googleButtonText: 'Login with Google',
      onGoogleButtonClick: () => this.redirectToGoogleLogin(),
      emailPasswordButtonText: 'Login with Email',
      onEmailPasswordButtonClick: () => this.redirectToEmailLogin(),
    });
  }

  getSignupButtons(): React.ReactElement {
    return Auth0Buttons({
      enableEmailPassword: !!AUTH_EMAIL_PASSWORD_CONN,
      googleButtonText: 'Sign-up with Google',
      onGoogleButtonClick: () => this.redirectToGoogleSignup(),
      emailPasswordButtonText: 'Sign-up with Email',
      onEmailPasswordButtonClick: () => this.redirectToEmailSignup(),
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
