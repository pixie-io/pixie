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

import {
  AUTH_URI, AUTH_CLIENT_ID,
} from 'app/containers/constants';
import ClientOAuth2 from 'client-oauth2';
import { PublicApiFactory } from '@ory/kratos-client';
import { FormStructure } from 'app/components';
import * as QueryString from 'query-string';
import { HydraInvitationForm } from 'app/containers/admin/hydra-invitation-form';
import { HydraButtons, RejectHydraSignup } from 'app/containers/auth/hydra-buttons';
import { OAuthProviderClient, Token } from './oauth-provider';

// Copied from auth0-js/src/helper/window.js
function randomString(length) {
  // eslint-disable-next-line no-var
  var bytes = new Uint8Array(length);
  const result = [];
  const charset = '0123456789ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyz-._~';

  const cryptoObj = window.crypto;
  if (!cryptoObj) {
    return null;
  }

  const random = cryptoObj.getRandomValues(bytes);

  for (let a = 0; a < random.length; a++) {
    result.push(charset[random[a] % charset.length]);
  }

  return result.join('');
}

const hydraStorageKey = 'hydra_auth_state';
export const PasswordError = new Error('Kratos identity server error: Password method not found in flows.');
export const FlowIDError = new Error('Auth server requires a flow parameter in the query string, but none were found.');

const kratosClient = PublicApiFactory(null, '/oauth/kratos');

// Renders a form with an error and no fields.
const displayErrorFormStructure = (error: Error): FormStructure => ({
  action: '/',
  method: 'POST',
  submitBtnText: 'Back To Login',
  fields: [],
  errors: [{ text: error.message }],
  defaultSubmit: false,
  onClick: () => {
    window.location.href = '/auth/login';
  },
});

export class HydraClient extends OAuthProviderClient {
  getRedirectURL: (boolean) => string;

  hydraStorageKey: string;

  constructor(getRedirectURL: (boolean) => string) {
    super();
    this.getRedirectURL = getRedirectURL;
    this.hydraStorageKey = hydraStorageKey;
  }

  loginRequest(): void {
    window.location.href = this.makeClient(this.makeAndStoreState(), /* isSignup */ false).token.getUri();
  }

  signupRequest(): void {
    window.location.href = this.makeClient(this.makeAndStoreState(), /* isSignup */ true).token.getUri();
  }

  handleToken(): Promise<Token> {
    return new Promise<Token>((resolve, reject) => {
      this.makeClient(this.getStoredState(), false).token.getToken(window.location).then((user) => {
        resolve({ accessToken: user.accessToken, isEmailUnverified: false });
      }).catch((err) => reject(err));
    });
  }

  // Get the PasswordLoginFlow from Kratos.
  // eslint-disable-next-line class-methods-use-this
  async getPasswordLoginFlow(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const flow = parsed.flow as string;
    if (flow == null) {
      return displayErrorFormStructure(FlowIDError);
    }
    const { data } = await kratosClient.getSelfServiceLoginFlow(flow);
    const passwordMethods = data.methods.password;
    if (passwordMethods == null) {
      return displayErrorFormStructure(PasswordError);
    }

    return {
      ...passwordMethods.config,
      submitBtnText: 'Login',
      errors: passwordMethods.config.messages,
      // Kratos and browser redirects limits us to submit the login form
      // through an XmlHttpRequest, the default HTML Form submit behavior.
      defaultSubmit: true,
    };
  }

  // eslint-disable-next-line class-methods-use-this
  async getResetPasswordFlow(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const flow = parsed.flow as string;
    if (flow == null) {
      return displayErrorFormStructure(FlowIDError);
    }
    const { data } = await kratosClient.getSelfServiceSettingsFlow(flow);
    const passwordMethods = data.methods.password;
    if (passwordMethods == null) {
      return displayErrorFormStructure(PasswordError);
    }

    return {
      ...passwordMethods.config,
      submitBtnText: 'Change Password',
      errors: passwordMethods.config.messages,
      // Kratos and browser redirects limits us to submit the login form
      // through an XmlHttpRequest, the default HTML Form submit behavior.
      defaultSubmit: true,
    };
  }

  // eslint-disable-next-line class-methods-use-this
  async getError(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const error = parsed.error as string;
    if (error == null) {
      return displayErrorFormStructure(new Error('server error'));
    }
    const { data } = await kratosClient.getSelfServiceError(error);

    return displayErrorFormStructure(new Error(JSON.stringify(data)));
  }

  // eslint-disable-next-line class-methods-use-this
  isInvitationEnabled(): boolean {
    return true;
  }

  // eslint-disable-next-line class-methods-use-this
  getInvitationComponent(): React.FC {
    return HydraInvitationForm;
  }

  getLoginButtons(): React.ReactElement {
    return HydraButtons({
      onUsernamePasswordButtonClick: () => this.loginRequest(),
      usernamePasswordText: 'Login',
    });
  }

  // eslint-disable-next-line class-methods-use-this
  getSignupButtons(): React.ReactElement {
    return RejectHydraSignup({});
  }

  private makeAndStoreState(): string {
    const state = randomString(48);
    window.localStorage.setItem(this.hydraStorageKey, state);
    return state;
  }

  private getStoredState(): string {
    const state = window.localStorage.getItem(this.hydraStorageKey);
    if (state != null) {
      return state;
    }

    throw new Error('OAuth state not found. Please try logging in again.');
  }

  private makeClient(state: string, isSignup: boolean): ClientOAuth2 {
    return new ClientOAuth2({
      clientId: AUTH_CLIENT_ID,
      authorizationUri: `https://${window.location.host}/${AUTH_URI}`,
      redirectUri: this.getRedirectURL(isSignup),
      scopes: ['vizier'],
      state,
    });
  }
}
