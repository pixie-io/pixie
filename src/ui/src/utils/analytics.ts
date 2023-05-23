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

/// <reference types="@types/segment-analytics" />

import { gql } from '@apollo/client';

import { PixieAPIClient, PixieAPIManager } from 'app/api';
import { isPixieEmbedded } from 'app/common/embed-context';
import { ANALYTICS_ENABLED, SEGMENT_UI_WRITE_KEY } from 'app/containers/constants';
import { GQLUserInfo, GQLUserSettings } from 'app/types/schema';
import { isValidAnalytics } from 'app/utils/env';

declare global {
  interface Window {
    analytics: SegmentAnalytics.AnalyticsJS;
    __pixie_cloud_version__: string;
  }
}

// Use this class instead of `window.analytics`.
// This should be the only user of window.analytics.
class PixieAnalytics {
  waiting = false;

  loaded = false;

  loggedIn = false;

  optOut = false;

  /**
   * Called automatically when PixieAnalytics is imported.
   *
   * Should be called manually whenever user logs in/out or opts in/out of analytics, to reset ID and permissions.
   */
  async reinit() {
    // Due to a timing issue with lazy evaluation and how Webpack exports simple classes, we have to wait just a brief
    // moment for PixieAPIManager to be defined. There is no cyclical dependency here, just a timing problem.
    // This does not affect other consumers: all of them happen to wait (for React, or for history events, etc) already.
    // This consumer, however, uses PixieAPIManager the moment this file is imported, and triggers the issue.
    await Promise.resolve();
    if (!PixieAPIManager) return; // Happens in unit tests for the same reason - just importing it causes issues.

    try {
      this.loggedIn = await PixieAPIManager.instance.isAuthenticated();
    } catch {
      this.loggedIn = false;
    }

    this.optOut = await this.checkOptOut();
    if (!this.optOut) {
      this.load();
      await this.autoIdentify();
    } else {
      if (this.loaded) {
        window.analytics.reset();
      }
    }
  }

  private async autoIdentify() {
    if (!this.loggedIn) return;

    const gqlClient = (PixieAPIManager.instance as PixieAPIClient).getCloudClient().graphQL;
    try {
      const { data: { user } } = await gqlClient.query<{
        user: Pick<GQLUserInfo, 'id' | 'email' | 'orgName' >,
      }>({
        query: gql`
          query getUserIdentityForAnalytics{
            user {
              id
              email
            }
          }
        `,
      });

      if (user?.id && user?.email) {
        const allowDefaultLauncher = !isPixieEmbedded()
          && ['/auth/login', '/auth/signup', '/login', '/signup'].includes(location.pathname);
        window.analytics.identify(
          user.id,
          { email: user.email },
          { integrations: { Intercom: { hideDefaultLauncher: !allowDefaultLauncher } } },
        );
      }
    } catch {
      // No-op: this happens when the user is not logged in; it's not an error.
    }
  }

  private async checkOptOut() {
    // To check if analytics are permitted to run, we test if there is a logged in user. If there is, check if they've
    // opted out of analytics. If there isn't, they're permitted until that changes (for things like login/signup flow).
    this.waiting = true;
    if (!this.loggedIn) {
      this.waiting = false;
      return false;
    }

    const gqlClient = (PixieAPIManager.instance as PixieAPIClient).getCloudClient().graphQL;
    const { data: { userSettings } } = await gqlClient.query<{ userSettings: GQLUserSettings }>({
      query: gql`
        query getUserOptOutSetting{
          userSettings {
            id
            analyticsOptout
          }
        }
      `,
    });

    this.waiting = false;
    return userSettings.analyticsOptout;
  }

  private get active() {
    return this.loaded && !this.waiting && !this.optOut;
  }

  get page() {
    return this.active ? window.analytics.page : () => {};
  }

  get track() {
    return this.active ? window.analytics.track : () => {};
  }

  get identify() {
    return this.active ? window.analytics.identify : () => {};
  }

  get alias() {
    return this.active ? window.analytics.alias : () => {};
  }

  get reset() {
    return this.active ? window.analytics.reset : () => {};
  }

  disable() {
    this.optOut = true;
  }

  enable() {
    this.optOut = false;
  }

  load() {
    if (this.loaded || this.optOut) {
      return;
    }
    if (ANALYTICS_ENABLED && isValidAnalytics()) {
      this.loaded = true;
      window.analytics.load(SEGMENT_UI_WRITE_KEY);
    }
  }
}

const instance = new PixieAnalytics();
instance.reinit().then();
export default instance;
