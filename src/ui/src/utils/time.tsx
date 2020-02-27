import * as moment from 'moment';

export function relativeTime(time: Date): string {
  return moment(time).fromNow();
}

export function nanoToMilliSeconds(t: number): number {
  return Math.floor(t / 1000000);
}

export function milliToNanoSeconds(t: number): number {
  return t * 1000000;
}
