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
import { OAUTH_PROVIDER } from 'app/containers/constants';
import Cookies from 'universal-cookie';
import { HydraClient } from './hydra-oauth-provider';
import { Auth0Client } from './auth0-oauth-provider';
import { OAuthProviderClient } from './oauth-provider';

export type AuthCallbackMode = 'cli_get' | 'cli_token' | 'ui';
const CSRF_COOKIE_NAME = 'csrf-cookie';

const cookies = new Cookies();

interface RedirectArgs {
  mode?: AuthCallbackMode;
  location?: string;
  org_name?: string;
  signup?: boolean;
  redirect_uri?: string;
}

const getRedirectURL = (isSignup: boolean) => {
  // Translate the old API parameters to new versions. In paricular:
  // local, (no redirect_url) -> cli_token
  // local -> cli_get
  // default: ui
  // We also translate the location parameters so redirects work as expected.
  // TODO(zasgar/michelle): When we finish porting everything to the new API this code
  // can be simplified.

  const redirectArgs: RedirectArgs = {};
  const parsed = QueryString.parse(window.location.search.substring(1));
  if (parsed.local_mode && !!parsed.local_mode) {
    if (parsed.redirect_uri) {
      redirectArgs.redirect_uri = typeof parsed.redirect_uri === 'string' && String(parsed.redirect_uri);
      redirectArgs.mode = 'cli_get';
    } else {
      redirectArgs.mode = 'cli_token';
    }
  } else {
    if (parsed.redirect_uri && typeof parsed.redirect_uri === 'string') {
      redirectArgs.redirect_uri = String(parsed.redirect_uri);
    }
    redirectArgs.mode = 'ui';
  }

  if (parsed.location && typeof parsed.location === 'string') {
    redirectArgs.location = parsed.location;
  }
  if (parsed.org_name && typeof parsed.org_name === 'string') {
    redirectArgs.org_name = parsed.org_name;
  }
  if (isSignup) {
    redirectArgs.signup = true;
  }
  const qs = QueryString.stringify(redirectArgs as any);
  const redirectURL = `${window.location.origin}/auth/callback?${qs}`;

  const segmentId = typeof parsed.tid === 'string' ? parsed.tid : '';
  if (segmentId) {
    analytics.alias(segmentId);
  }
  return redirectURL;
};

export const GetOAuthProvider = (): OAuthProviderClient => {
  if (OAUTH_PROVIDER === 'auth0') {
    return new Auth0Client(getRedirectURL);
  }
  if (OAUTH_PROVIDER === 'hydra') {
    return new HydraClient(getRedirectURL);
  }
  throw new Error(`OAUTH_PROVIDER ${OAUTH_PROVIDER} invalid. Expected hydra or auth0.`);
};

export const GetCSRFCookie = (): string => (cookies.get(CSRF_COOKIE_NAME));
