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
import { PIXIE_CLOUD_VERSION, isValidAnalytics } from 'app/utils/env';

declare global {
  interface Window {
    analytics: SegmentAnalytics.AnalyticsJS;
    __pixie_cloud_version__: string;
  }
}

class Analytics {
  constructor() {
    // If the key is not valid, we disable segment.
    if (ANALYTICS_ENABLED && isValidAnalytics()) {
      this.load();
    }
  }

  // eslint-disable-next-line class-methods-use-this
  get page() {
    return window.analytics.page;
  }

  // eslint-disable-next-line class-methods-use-this
  get track() {
    return window.analytics.track;
  }

  // eslint-disable-next-line class-methods-use-this
  get identify() {
    return window.analytics.identify;
  }

  // eslint-disable-next-line class-methods-use-this
  get alias() {
    return window.analytics.alias;
  }

  // eslint-disable-next-line class-methods-use-this
  load() {
    // eslint-disable-next-line no-underscore-dangle
    window.__pixie_cloud_version__ = PIXIE_CLOUD_VERSION;
    window.analytics.load(SEGMENT_UI_WRITE_KEY);
  }
}

export default new Analytics();
