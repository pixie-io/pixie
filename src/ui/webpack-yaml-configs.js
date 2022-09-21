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
  PL_AUTH_URI: 'oauth/hydra/oauth2/auth',
  PL_AUTH_CLIENT_ID: 'auth-code-client',
  PL_AUTH_EMAIL_PASSWORD_CONN: '',
};

const LD_DEFAULTS = {
  PL_LD_CLIENT_ID: '',
  PL_LD_SDK_KEY: '',
};

const ANNOUNCEMENT_DEFAULTS = {
  ANNOUNCEMENT_ENABLED: false,
  ANNOUNCE_WIDGET_URL: '',
};

/**
 * Used in the webpack dev server to configure things like oauth, third-party services, and others that are
 * normally handled by other processes in a deployed environment.
 *
 * @param {string} topLevelDir Top of git tree
 * @param {string} environment One of 'base', 'dev', 'staging', etc.
 */
module.exports = function getYamls(topLevelDir, environment) {
  const credentialsEnv = process.env.PL_BUILD_TYPE;
  const oauthConfigEnv = process.env.PL_OAUTH_CONFIG_ENV;

  // Users can specify the OAUTH environment. Usually this just means
  // setting to "ory_auth", otherwise will default to `environment`.
  const oauthPath = oauthConfigEnv === 'ory-auth'
    ? join(topLevelDir, 'k8s', 'cloud', 'base', oauthConfigEnv, 'oauth_config.yaml')
    : join(topLevelDir, 'private', 'credentials', 'k8s', credentialsEnv, 'configs', 'oauth_config.yaml');
  let oauthYAML;
  try {
    oauthYAML = utils.readYAMLFile(oauthPath, true);
  } catch (_) {
    oauthYAML = { data: OAUTH_DEFAULTS };
  }

  // Get client ID for LaunchDarkly, if applicable.
  let ldYAML;
  try {
    ldYAML = utils.readYAMLFile(join(topLevelDir, 'private', 'credentials', 'k8s',
      credentialsEnv, 'configs', 'ld_config.yaml'), true);
  } catch (_) {
    ldYAML = { data: LD_DEFAULTS };
  }

  // Get domain name.
  const domainYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment, 'domain_config.yaml'), false);

  // Get whether to enable analytics.
  const analyticsYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment,
    'analytics_config.yaml'), false);

  // Get whether to enable announcekit.
  let announcementYAML;
  try {
    announcementYAML = utils.readYAMLFile(join(topLevelDir, 'private', 'credentials', 'k8s',
      credentialsEnv, 'configs', 'announce_config.yaml'), true);
  } catch (_) {
    announcementYAML = { data: ANNOUNCEMENT_DEFAULTS };
  }

  // Get whether to enable chat contact.
  const contactYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment,
    'contact_config.yaml'), false);

  // Get URLs where PxL scripts can be found.
  const scriptBundleYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment,
    'script_bundles_config.yaml'), false);

  return {
    oauthYAML,
    ldYAML,
    domainYAML,
    analyticsYAML,
    announcementYAML,
    contactYAML,
    scriptBundleYAML,
  };
};
