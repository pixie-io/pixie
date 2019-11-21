/// <reference types="@types/segment-analytics" />

import {SEGMENT_UI_WRITE_KEY} from 'containers/constants';

declare global {
    interface Window { analytics: SegmentAnalytics.AnalyticsJS; }
}

class Analytics {
    constructor() {
        if (SEGMENT_UI_WRITE_KEY) {
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
