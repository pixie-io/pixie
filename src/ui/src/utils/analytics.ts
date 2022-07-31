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

import { ANALYTICS_ENABLED, SEGMENT_UI_WRITE_KEY } from 'app/containers/constants';
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
  loaded = false;

  optOut = false;

  private get active() {
    return this.loaded && !this.optOut;
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

export default new PixieAnalytics();
