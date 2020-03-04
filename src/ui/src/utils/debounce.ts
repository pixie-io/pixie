export function debounce(func: (...args: Array<{}>) => void, wait: number) {
  let timeout;
  let context;
  let lastArgs;
  return function call(...args: Array<{}>) {
    context = this;
    lastArgs = args;
    const onTimeout = () => {
      func.apply(context, lastArgs);
      timeout = null;
    };
    const newTimeout = setTimeout(onTimeout, wait);
    if (timeout) {
      // Restart the timer if one already exists.
      clearTimeout(timeout);
    }
    timeout = newTimeout;
  };
}
