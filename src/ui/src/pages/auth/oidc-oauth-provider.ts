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
import {
  OIDC_CLIENT_ID, OIDC_HOST, OIDC_METADATA_URL, OIDC_ADDITIONAL_SCOPES,
  OIDC_SOCIAL_CONFIG_LOGIN, OIDC_SOCIAL_CONFIG_SIGNUP,
} from 'app/containers/constants';
import pixieAnalytics from 'app/utils/analytics';

import { CallbackArgs, getLoginArgs, getSignupArgs } from './callback-url';

const COMMON_SCOPES = 'openid profile email';

export const OIDCClient = {
  userManager: new UserManager({
    authority: OIDC_HOST,
    metadataUrl: OIDC_METADATA_URL,
    client_id: OIDC_CLIENT_ID,
    loadUserInfo: false,
    redirect_uri: `${window.location.origin}/auth/callback`,
    scope: OIDC_ADDITIONAL_SCOPES ? `${OIDC_ADDITIONAL_SCOPES} ${COMMON_SCOPES}` : COMMON_SCOPES,
    response_type: 'token id_token',
  }),

  redirectToLogin(extraQueryParams: Record<string, any>): void {
    const args = getLoginArgs();

    if (args.invite_token) {
      extraQueryParams.invite = true;
    }

    pixieAnalytics.track('Redirect to login', { args, extraQueryParams });
    this.userManager.signinRedirect({
      extraQueryParams,
      prompt: 'login',
      state: {
        redirectArgs: args,
      },
    });
  },

  redirectToSignup(extraQueryParams: Record<string, any>): void {
    const args = getSignupArgs();

    if (args.invite_token) {
      extraQueryParams.invite = true;
    }

    pixieAnalytics.track('Redirect to signup', { args, extraQueryParams });
    this.userManager.signinRedirect({
      extraQueryParams,
      prompt: 'login',
      state: {
        redirectArgs: args,
      },
    });
  },

  refetchToken(): void {
    this.userManager.signinRedirect({
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  handleToken(): Promise<CallbackArgs> {
    return new Promise<CallbackArgs>((resolve, reject) => {
      // The callback doesn't require any settings to be created.
      // That means this implementation is agnostic to the OIDC that we connected to.
      this.userManager.signinRedirectCallback()
        .then((user) => {
          if (!user) {
            reject(new Error('user is undefined, please try logging in again'));
          }
          resolve({
            redirectArgs: user.state.redirectArgs,
            token: {
              accessToken: user.access_token,
              idToken: user.id_token,
            },
          });
        }).catch(reject);
    });
  },

  async getPasswordLoginFlow(): Promise<FormStructure> {
    throw new Error('Password flow not available for OIDC. Use the proper OIDC flow.');
  },

  async getResetPasswordFlow(): Promise<FormStructure> {
    throw new Error('Reset Password flow not available for OIDC. Use the proper OIDC flow.');
  },

  getLoginButtons(): React.ReactElement {
    const defaultConfig = [{ buttonText: 'Login', extraParams: {} }];
    let parsedConfig;
    try {
      parsedConfig = JSON.parse(OIDC_SOCIAL_CONFIG_LOGIN);
    } catch {
      // Do nothing, we'll just use default in this case.
    }
    const configs = Array.isArray(parsedConfig) ? parsedConfig : defaultConfig;
    return OIDCButtons({
      configs,
      onButtonClick: (params) => this.redirectToLogin(params),
    });
  },

  getSignupButtons(): React.ReactElement {
    const defaultConfig = [{ buttonText: 'Sign-Up', extraParams: {} }];
    let parsedConfig;
    try {
      parsedConfig = JSON.parse(OIDC_SOCIAL_CONFIG_SIGNUP);
    } catch {
      // Do nothing, we'll just use default in this case.
    }
    const configs = Array.isArray(parsedConfig) ? parsedConfig : defaultConfig;
    return OIDCButtons({
      configs,
      onButtonClick: (params) => this.redirectToSignup(params),
    });
  },

  async getError(): Promise<FormStructure> {
    throw new Error('error flow not supported for OIDC');
  },

  isInvitationEnabled(): boolean {
    return false;
  },

  getInvitationComponent(): React.FC {
    return undefined;
  },
};
