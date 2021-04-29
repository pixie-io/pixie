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

/*  eslint-disable class-methods-use-this */
import type * as React from 'react';

import auth0 from 'auth0-js';
import { AUTH_CLIENT_ID, AUTH_URI } from 'containers/constants';
import { FormStructure } from '@pixie-labs/components';
import { Auth0Buttons } from 'containers/auth/auth0-buttons';
import { OAuthProviderClient, Token } from './oauth-provider';

export class Auth0Client extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
  }

  googleLoginRequest(): void {
    this.makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ false),
      prompt: 'login',
    });
  }

  googleSignupRequest(): void {
    this.makeAuth0Client().authorize({
      connection: 'google-oauth2',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ true),
      prompt: 'login',
    });
  }

  usernameLoginRequest(): void {
    this.makeAuth0Client().authorize({
      connection: 'Username-Password-Authentication',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ false),
      prompt: 'login',
      mode: 'login',
    });
  }

  usernameSignupRequest(): void {
    this.makeAuth0Client().authorize({
      connection: 'Username-Password-Authentication',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ true),
      prompt: 'login',
      mode: 'signUp',
    });
  }

  usernameSignupCompleteRequest(): void {
    this.makeAuth0Client().authorize({
      connection: 'Username-Password-Authentication',
      responseType: 'token',
      redirectUri: this.getRedirectURL(/* isSignup */ true),
      prompt: 'none',
      mode: 'signUp',
    });
  }

  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      this.makeAuth0Client().parseHash({ hash: window.location.hash }, (errStatus, authResult) => {
        if (errStatus) {
          reject(new Error(`${errStatus.error} - ${errStatus.errorDescription}`));
          return;
        }
        resolve(authResult.accessToken);
      });
    });
  }

  async getPasswordLoginFlow(): Promise<FormStructure> {
    throw new Error('Password flow currently unavailable for Auth0');
  }

  async getResetPasswordFlow(): Promise<FormStructure> {
    throw new Error('Reset password flow currently unavailable for Auth0');
  }

  getLoginButtons(): React.ReactElement {
    return Auth0Buttons({
      action: 'Login',
      onGoogleButtonClick: () => this.googleLoginRequest(),
      onUsernamePasswordButtonClick: () => this.usernameLoginRequest(),
    });
  }

  getSignupButtons(): React.ReactElement {
    return Auth0Buttons({
      action: 'Sign-Up',
      onGoogleButtonClick: () => this.googleSignupRequest(),
      onUsernamePasswordButtonClick: () => this.usernameSignupRequest(),
    });
  }

  async getError(): Promise<FormStructure> {
    throw new Error('error flow not supported for Auth0');
  }

  isInvitationEnabled(): boolean {
    return false;
  }

  getInvitationComponent(): React.FC {
    return undefined;
  }

  private makeAuth0Client(): auth0.WebAuth {
    return new auth0.WebAuth({
      domain: AUTH_URI,
      clientID: AUTH_CLIENT_ID,
    });
  }
}
