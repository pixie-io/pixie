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
  loaded: boolean;

  optOut: boolean;

  constructor() {
    this.loaded = false;
    this.optOut = false;
  }

  // eslint-disable-next-line class-methods-use-this
  get page() {
    if (this.optOut) {
      return () => {};
    }
    return window.analytics.page;
  }

  // eslint-disable-next-line class-methods-use-this
  get track() {
    if (this.optOut) {
      return () => {};
    }
    return window.analytics.track;
  }

  // eslint-disable-next-line class-methods-use-this
  get identify() {
    if (this.optOut) {
      return () => {};
    }
    return window.analytics.identify;
  }

  // eslint-disable-next-line class-methods-use-this
  get alias() {
    if (this.optOut) {
      return () => {};
    }
    return window.analytics.alias;
  }

  // eslint-disable-next-line class-methods-use-this
  get reset() {
    if (this.optOut) {
      return () => {};
    }
    return window.analytics.reset;
  }

  disable() {
    this.optOut = true;
  }

  enable() {
    this.optOut = false;
  }

  // eslint-disable-next-line class-methods-use-this
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
