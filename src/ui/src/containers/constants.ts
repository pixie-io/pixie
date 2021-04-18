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

declare global {
  type OAuthProvider = 'hydra' | 'auth0';
  interface Window {
    __PIXIE_FLAGS__: {
      OAUTH_PROVIDER: OAuthProvider;
      AUTH_URI: string;
      AUTH_CLIENT_ID: string;
      DOMAIN_NAME: string;
      SEGMENT_UI_WRITE_KEY: string;
      LD_CLIENT_ID: string;
      ANALYTICS_ENABLED: boolean;
      ANNOUNCEMENT_ENABLED: boolean;
      ANNOUNCE_WIDGET_URL: string;
      CONTACT_ENABLED: boolean;
    };
  }
}

// eslint-disable-next-line no-underscore-dangle
export const { OAUTH_PROVIDER } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { AUTH_URI } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { AUTH_CLIENT_ID } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { DOMAIN_NAME } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { SEGMENT_UI_WRITE_KEY } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { LD_CLIENT_ID } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANALYTICS_ENABLED } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANNOUNCEMENT_ENABLED } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { ANNOUNCE_WIDGET_URL } = window.__PIXIE_FLAGS__;
// eslint-disable-next-line no-underscore-dangle
export const { CONTACT_ENABLED } = window.__PIXIE_FLAGS__;
