export const resizeEvent = new Event('resize');

let called = false;

export function triggerResize() {
  if (called) {
    return;
  }
  called = true;
  setTimeout(() => {
    window.dispatchEvent(resizeEvent);
    called = false;
  });
}
