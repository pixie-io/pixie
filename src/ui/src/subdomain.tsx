import './index.scss';

import {SubdomainApp} from 'containers/subdomain-app';
import * as moment from 'moment';
import * as React from 'react';
import * as ReactDOM from 'react-dom';

moment.updateLocale('en', {
  relativeTime: {
    s: 'seconds',
    ss: '%d sec',
    m: 'a minute',
    mm: '%d min',
    h: 'an hour',
    hh: '%d hrs',
    d: 'a day',
    dd: '%d days',
    M: 'a month',
    MM: '%d months',
    y: 'a year',
    yy: '%d years',
  },
});

ReactDOM.render(<SubdomainApp />, document.getElementById('root'));
