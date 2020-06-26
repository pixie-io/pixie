/// <reference types="@types/segment-analytics" />

import { SEGMENT_UI_WRITE_KEY } from 'containers/constants';
import { PIXIE_CLOUD_VERSION } from 'utils/env';

declare global {
  interface Window {
    analytics: SegmentAnalytics.AnalyticsJS;
    __pixie_cloud_version__: string;
  }
}

function isValidSegmentKey(k) {
  // The TS compiler is really smart and is optmizing away the checks,
  // which is why this check is so convoluted...
  return k && !k.startsWith('__S');
}

class Analytics {
  constructor() {
    // If the key is not valid, we disable segment.
    if (isValidSegmentKey(SEGMENT_UI_WRITE_KEY)) {
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
    // eslint-disable-next-line @typescript-eslint/camelcase,no-underscore-dangle
    window.__pixie_cloud_version__ = PIXIE_CLOUD_VERSION;
    window.analytics.load(SEGMENT_UI_WRITE_KEY);
  }
}

export default new Analytics();
