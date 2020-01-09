import * as moment from 'moment';

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

export function relativeTime(time: Date): string {
  return moment(time).fromNow();
}
