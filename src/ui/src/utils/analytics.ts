/// <reference types="@types/segment-analytics" />

import {SEGMENT_UI_WRITE_KEY} from 'containers/constants';

declare global {
    interface Window { analytics: SegmentAnalytics.AnalyticsJS; }
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

    load = () => {
        window.analytics.load(SEGMENT_UI_WRITE_KEY);
    }

    page = () => {
        window.analytics.page();
    }

    track = (name: string, properties: any) => {
        window.analytics.track(name, properties);
    }

    identify = (userid: string, traits: any) => {
        window.analytics.identify(userid, traits);
    }
}

export default new Analytics();
