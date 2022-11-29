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
  type OAuthProvider = 'hydra' | 'auth0' | 'oidc';
  interface Window {
    __PIXIE_FLAGS__: {
      OAUTH_PROVIDER: OAuthProvider;
      AUTH_URI: string;
      AUTH_CLIENT_ID: string;
      AUTH_EMAIL_PASSWORD_CONN: string;
      OIDC_HOST: string;
      OIDC_METADATA_URL: string;
      OIDC_CLIENT_ID: string;
      OIDC_ADDITIONAL_SCOPES: string;
      OIDC_SOCIAL_CONFIG_LOGIN: string;
      OIDC_SOCIAL_CONFIG_SIGNUP: string;
      DOMAIN_NAME: string;
      SEGMENT_UI_WRITE_KEY: string;
      LD_CLIENT_ID: string;
      SCRIPT_BUNDLE_URLS: string; // Actually a string[] in JSON form
      SCRIPT_BUNDLE_DEV: boolean;
      ANALYTICS_ENABLED: boolean;
      ANNOUNCEMENT_ENABLED: boolean;
      ANNOUNCE_WIDGET_URL: string;
      CONTACT_ENABLED: boolean;
      PASSTHROUGH_PROXY_PORT: string;
    };
  }
}

/* eslint-disable no-underscore-dangle */
export const { OAUTH_PROVIDER } = window.__PIXIE_FLAGS__;
export const { AUTH_URI } = window.__PIXIE_FLAGS__;
export const { AUTH_CLIENT_ID } = window.__PIXIE_FLAGS__;
export const { AUTH_EMAIL_PASSWORD_CONN } = window.__PIXIE_FLAGS__;
export const { OIDC_HOST } = window.__PIXIE_FLAGS__;
export const { OIDC_METADATA_URL } = window.__PIXIE_FLAGS__;
export const { OIDC_CLIENT_ID } = window.__PIXIE_FLAGS__;
export const { OIDC_ADDITIONAL_SCOPES } = window.__PIXIE_FLAGS__;
export const { OIDC_SOCIAL_CONFIG_LOGIN } = window.__PIXIE_FLAGS__;
export const { OIDC_SOCIAL_CONFIG_SIGNUP } = window.__PIXIE_FLAGS__;
export const { DOMAIN_NAME } = window.__PIXIE_FLAGS__;
export const { SEGMENT_UI_WRITE_KEY } = window.__PIXIE_FLAGS__;
export const { LD_CLIENT_ID } = window.__PIXIE_FLAGS__;
export const { SCRIPT_BUNDLE_URLS } = window.__PIXIE_FLAGS__;
export const { SCRIPT_BUNDLE_DEV } = window.__PIXIE_FLAGS__;
export const { ANALYTICS_ENABLED } = window.__PIXIE_FLAGS__;
export const { ANNOUNCEMENT_ENABLED } = window.__PIXIE_FLAGS__;
export const { ANNOUNCE_WIDGET_URL } = window.__PIXIE_FLAGS__;
export const { CONTACT_ENABLED } = window.__PIXIE_FLAGS__;
export const { PASSTHROUGH_PROXY_PORT } = window.__PIXIE_FLAGS__;
/* eslint-enable no-underscore-dangle */
