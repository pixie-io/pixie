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

import { PublicApiFactory } from '@ory/kratos-client';
import { UserManager } from 'oidc-client';
import * as QueryString from 'query-string';

import { FormStructure } from 'app/components';
import { HydraInvitationForm } from 'app/containers/admin/hydra-invitation-form';
import { HydraButtons, RejectHydraSignup } from 'app/containers/auth/hydra-buttons';
import { AUTH_CLIENT_ID, AUTH_URI } from 'app/containers/constants';

import { CallbackArgs, getSignupArgs, getLoginArgs } from './callback-url';

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

export const HydraClient = {
  userManager: new UserManager({
    authority: AUTH_URI,
    client_id: AUTH_CLIENT_ID,
    redirect_uri: `${window.location.origin}/auth/callback`,
    scope: 'vizier',
    response_type: 'token',
  }),

  loginRequest(): void {
    this.userManager.signinRedirect({
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  signupRequest(): void {
    this.userManager.signinRedirect({
      state: {
        redirectArgs: getSignupArgs(),
      },
    });
  },

  refetchToken(): void {
    this.userManager.signinSilent({
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  handleToken(): Promise<CallbackArgs> {
    return new Promise<CallbackArgs>((resolve, reject) => {
      this.userManager.signinRedirectCallback()
        .then((user) => {
          if (!user) {
            reject(new Error('user is undefined, please try logging in again'));
          }
          resolve({
            redirectArgs: user.state.redirectArgs,
            token: {
              accessToken: user.access_token,
            },
          });
        }).catch(reject);
    });
  },

  // Get the PasswordLoginFlow from Kratos.
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
  },

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
  },

  async getError(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const error = parsed.error as string;
    if (error == null) {
      return displayErrorFormStructure(new Error('server error'));
    }
    const { data } = await kratosClient.getSelfServiceError(error);

    return displayErrorFormStructure(new Error(JSON.stringify(data)));
  },

  isInvitationEnabled(): boolean {
    return true;
  },

  getInvitationComponent(): React.FC {
    return HydraInvitationForm;
  },

  getLoginButtons(): React.ReactElement {
    return HydraButtons({
      onUsernamePasswordButtonClick: () => this.loginRequest(),
      usernamePasswordText: 'Login',
    });
  },

  getSignupButtons(): React.ReactElement {
    return RejectHydraSignup({});
  },
};
