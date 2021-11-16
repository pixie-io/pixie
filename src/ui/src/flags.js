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

/* global __CONFIG_OAUTH_PROVIDER__, __CONFIG_AUTH_CLIENT_ID__,
__CONFIG_AUTH_URI__, __CONFIG_AUTH_EMAIL_PASSWORD_CONN__, __CONFIG_DOMAIN_NAME__,
__CONFIG_LD_CLIENT_ID__, __SEGMENT_UI_WRITE_KEY__, __ANALYTICS_ENABLED__,
__ANNOUNCEMENT_ENABLED__, __ANNOUNCE_WIDGET_URL__, __CONTACT_ENABLED__ */
const OAUTH_PROVIDER = __CONFIG_OAUTH_PROVIDER__;
const AUTH_URI = __CONFIG_AUTH_URI__;
const AUTH_CLIENT_ID = __CONFIG_AUTH_CLIENT_ID__;
const AUTH_EMAIL_PASSWORD_CONN = __CONFIG_AUTH_EMAIL_PASSWORD_CONN__;
const DOMAIN_NAME = __CONFIG_DOMAIN_NAME__;
const LD_CLIENT_ID = __CONFIG_LD_CLIENT_ID__;
const SEGMENT_UI_WRITE_KEY = __SEGMENT_UI_WRITE_KEY__;
const ANALYTICS_ENABLED = __ANALYTICS_ENABLED__;
const ANNOUNCEMENT_ENABLED = __ANNOUNCEMENT_ENABLED__;
const ANNOUNCE_WIDGET_URL = __ANNOUNCE_WIDGET_URL__;
const CONTACT_ENABLED = __CONTACT_ENABLED__;

// There is a bug with using esnext + webpack-replace-plugin, where
// lines with an export and replacement will not compile properly.
// eslint-disable-next-line no-underscore-dangle
window.__PIXIE_FLAGS__ = {
  OAUTH_PROVIDER,
  AUTH_URI,
  AUTH_CLIENT_ID,
  AUTH_EMAIL_PASSWORD_CONN,
  DOMAIN_NAME,
  SEGMENT_UI_WRITE_KEY,
  LD_CLIENT_ID,
  ANALYTICS_ENABLED,
  ANNOUNCEMENT_ENABLED,
  ANNOUNCE_WIDGET_URL,
  CONTACT_ENABLED,
};
