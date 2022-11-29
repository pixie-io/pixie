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

const { join } = require('path');

const utils = require('./webpack-utils');

const OAUTH_DEFAULTS = {
  PL_OAUTH_PROVIDER: 'hydra',
  PL_AUTH_URI: '/oauth/hydra',
  PL_AUTH_CLIENT_ID: 'auth-code-client',
  PL_AUTH_EMAIL_PASSWORD_CONN: '',
  PL_OIDC_HOST: '',
  PL_OIDC_METADATA_URL: '',
  PL_OIDC_CLIENT_ID: '',
  PL_OIDC_ADDITIONAL_SCOPES: '',
  PL_OIDC_SOCIAL_CONFIG_LOGIN: '',
  PL_OIDC_SOCIAL_CONFIG_SIGNUP: '',
};

const LD_DEFAULTS = {
  PL_LD_CLIENT_ID: '',
  PL_LD_SDK_KEY: '',
};

const ANNOUNCEMENT_DEFAULTS = {
  ANNOUNCEMENT_ENABLED: false,
  ANNOUNCE_WIDGET_URL: '',
};

function findConfig(root, environment, fileName, defaults, isEncrypted) {
  const overrides = JSON.parse(process.env.PL_CONFIG_DIRS || '[]'); // Assumes it's a JSON-stringified array of strings.
  for (const path of overrides) {
    try {
      return utils.readYAMLFile(join(root, ...path.split('/'), fileName), isEncrypted);
    } catch (_) { /* Just try the next one */ }
  }
  // If there were no overrides or none of them have the file we're looking for, try the default location.
  // If it isn't there either, use the supplied default values.
  try {
    return utils.readYAMLFile(join(root, 'k8s', 'cloud', environment, fileName), isEncrypted);
  } catch (_) {
    return { data: defaults };
  }
}

/**
 * Used in the webpack dev server to configure things like oauth, third-party services, and others that are
 * normally handled by other processes in a deployed environment.
 *
 * @param {string} root Top of git tree
 * @param {string} environment One of 'base', 'dev', 'staging', etc.
 */
function getYamls(root, environment) {
  // Users can specify the OAUTH environment. Usually this just means
  // setting to "ory_auth", otherwise will default to `environment`.
  const oauthConfigEnv = process.env.PL_OAUTH_CONFIG_ENV;
  const oauth = oauthConfigEnv === 'ory_auth'
    ? utils.readYAMLFile(join(root, 'k8s', 'cloud', 'base', oauthConfigEnv, 'oauth_config.yaml'), false)
    : findConfig(root, environment, 'oauth_config.yaml', OAUTH_DEFAULTS, true);

  // Get client ID for LaunchDarkly, if applicable.
  const ld = findConfig(root, environment, 'ld_config.yaml', LD_DEFAULTS, true);

  // Get domain name.
  const domain = findConfig(root, environment, 'domain_config.yaml', null, false);

  // Get whether to enable analytics.
  const analytics = findConfig(root, environment, 'analytics_config.yaml', null, false);

  // Get whether to enable announcekit.
  const announcement = findConfig(root, environment, 'announce_config.yaml', ANNOUNCEMENT_DEFAULTS, true);

  // Get whether to enable chat contact.
  const contact = findConfig(root, environment, 'contact_config.yaml', null, false);

  // Get URLs where PxL scripts can be found.
  const scriptBundle = findConfig(root, environment, 'script_bundles_config.yaml', null, false);

  return {
    oauth,
    ld,
    domain,
    analytics,
    announcement,
    contact,
    scriptBundle,
  };
}

module.exports = { findConfig, getYamls };
