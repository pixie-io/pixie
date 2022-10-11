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

import * as QueryString from 'query-string';

import pixieAnalytics from 'app/utils/analytics';

export type AuthCallbackMode = 'cli_get' | 'cli_token' | 'ui';

export interface RedirectArgs {
  mode?: AuthCallbackMode;
  signup?: boolean;
  redirect_uri?: string;
  invite_token?: string;
}

export type Token = {
  accessToken?: string;
  idToken?: string;
};

export type CallbackArgs = {
  redirectArgs: RedirectArgs;
  token: Token;
};

const getRedirectArgs = (isSignup: boolean): RedirectArgs => {
  // Translate the login parameters to type of login flow.
  // local_mode && (no redirect_uri) -> cli_token
  // local_mode && redirect_uri -> cli_get
  // default: ui

  const redirectArgs: RedirectArgs = {
    mode: 'ui',
  };
  const parsed = QueryString.parse(window.location.search);
  if (parsed.redirect_uri && typeof parsed.redirect_uri === 'string') {
    redirectArgs.redirect_uri = String(parsed.redirect_uri);
  }

  if (parsed.local_mode && !!redirectArgs.redirect_uri) {
    redirectArgs.mode = 'cli_get';
  } else if (parsed.local_mode) {
    redirectArgs.mode = 'cli_token';
  }

  if (parsed.invite_token && typeof parsed.invite_token === 'string') {
    redirectArgs.invite_token = parsed.invite_token;
  }

  if (isSignup) {
    redirectArgs.signup = true;
  }

  if (parsed.tid && typeof parsed.tid === 'string') {
    pixieAnalytics.alias(parsed.tid);
  }
  return redirectArgs;
};

export const getSignupArgs = () => getRedirectArgs(true);
export const getLoginArgs = () => getRedirectArgs(false);
