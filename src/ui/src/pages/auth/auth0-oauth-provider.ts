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
import { Auth0Buttons } from 'app/containers/auth/auth0-buttons';
import { AUTH_CLIENT_ID, AUTH_EMAIL_PASSWORD_CONN, AUTH_URI } from 'app/containers/constants';
import pixieAnalytics from 'app/utils/analytics';

import { getSignupArgs, CallbackArgs, getLoginArgs } from './callback-url';

export const Auth0Client = {
  userManager: new UserManager({
    authority: `https://${AUTH_URI}`,
    client_id: AUTH_CLIENT_ID,
    redirect_uri: `${window.location.origin}/auth/callback`,
    loadUserInfo: false,
    scope: 'openid profile email',
    response_type: 'token id_token',
  }),

  redirectToGoogleLogin(): void {
    pixieAnalytics.track('Redirect to Google login');
    this.userManager.signinRedirect({
      extraQueryParams: {
        connection: 'google-oauth2',
      },
      prompt: 'login',
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  redirectToGoogleSignup(): void {
    pixieAnalytics.track('Redirect to Google signup');
    this.userManager.signinRedirect({
      extraQueryParams: {
        connection: 'google-oauth2',
      },
      prompt: 'login',
      state: {
        redirectArgs: getSignupArgs(),
      },
    });
  },

  redirectToEmailLogin(): void {
    pixieAnalytics.track('Redirect to email login');
    this.userManager.signinRedirect({
      extraQueryParams: {
        connection: AUTH_EMAIL_PASSWORD_CONN,
        // Manually configured in Classic Universal Login settings.
        mode: 'login',
      },
      prompt: 'login',
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  redirectToEmailSignup(): void {
    pixieAnalytics.track('Redirect to email signup');
    this.userManager.signinRedirect({
      extraQueryParams: {
        connection: AUTH_EMAIL_PASSWORD_CONN,
        // Manually configured in Classic Universal Login settings.
        mode: 'signUp',
        // Used by New Universal Login https://auth0.com/docs/login/universal-login/new-experience#signup
        screen_hint: 'signup',
      },
      prompt: 'login',
      state: {
        // Even though we are in a signup flow, the callback shouldn't "sign up" the
        // user until verification is complete.
        redirectArgs: getLoginArgs(),
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
    return Auth0Buttons({
      enableEmailPassword: !!AUTH_EMAIL_PASSWORD_CONN,
      googleButtonText: 'Login with Google',
      onGoogleButtonClick: () => this.redirectToGoogleLogin(),
      emailPasswordButtonText: 'Login with Email',
      onEmailPasswordButtonClick: () => this.redirectToEmailLogin(),
    });
  },

  getSignupButtons(): React.ReactElement {
    return Auth0Buttons({
      enableEmailPassword: !!AUTH_EMAIL_PASSWORD_CONN,
      googleButtonText: 'Sign-up with Google',
      onGoogleButtonClick: () => this.redirectToGoogleSignup(),
      emailPasswordButtonText: 'Sign-up with Email',
      onEmailPasswordButtonClick: () => this.redirectToEmailSignup(),
    });
  },

  async getError(): Promise<FormStructure> {
    throw new Error('error flow not supported for Auth0');
  },

  isInvitationEnabled(): boolean {
    return false;
  },

  getInvitationComponent(): React.FC {
    return undefined;
  },
};
