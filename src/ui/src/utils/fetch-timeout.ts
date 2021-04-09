import fetch from 'cross-fetch';

export default function fetchWithTimeout(ms: number): typeof fetch {
  return (uri, options) => {
    let timeout;
    const timeoutP = new Promise<never>((_, reject) => {
      timeout = setTimeout(() => {
        reject(new Error('request timed out'));
      }, ms);
    });
    const fetchReq = fetch(uri, options).then((resp) => {
      clearTimeout(timeout);
      return resp;
    });
    return Promise.race([fetchReq, timeoutP]);
  };
}
