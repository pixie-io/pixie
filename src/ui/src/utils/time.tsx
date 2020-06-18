export function nanoToMilliSeconds(t: number): number {
  return Math.floor(t / 1000000);
}

export function milliToNanoSeconds(t: number): number {
  return t * 1000000;
}

export function nanoToSeconds(t: number): number {
  return t / 1000000000;
}
