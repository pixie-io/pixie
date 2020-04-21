export function isMac(): boolean {
  return window.navigator.platform.toLowerCase().indexOf('mac') >= 0;
}
