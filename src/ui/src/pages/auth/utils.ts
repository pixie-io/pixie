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

import Cookies from 'universal-cookie';

import { OAUTH_PROVIDER } from 'app/containers/constants';

import { Auth0Client } from './auth0-oauth-provider';
import { getRedirectURL } from './callback-url';
import { HydraClient } from './hydra-oauth-provider';
import { OAuthProviderClient } from './oauth-provider';

const CSRF_COOKIE_NAME = 'csrf-cookie';

const cookies = new Cookies();

// eslint-disable-next-line react-memo/require-memo
export const GetOAuthProvider = (): OAuthProviderClient => {
  if (OAUTH_PROVIDER === 'auth0') {
    return new Auth0Client(getRedirectURL);
  }
  if (OAUTH_PROVIDER === 'hydra') {
    return new HydraClient(getRedirectURL);
  }
  throw new Error(`OAUTH_PROVIDER ${OAUTH_PROVIDER} invalid. Expected hydra or auth0.`);
};

// eslint-disable-next-line react-memo/require-memo
export const GetCSRFCookie = (): string => (cookies.get(CSRF_COOKIE_NAME));
