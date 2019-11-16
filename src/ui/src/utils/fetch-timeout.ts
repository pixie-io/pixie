import {fetch} from 'whatwg-fetch';

export default function fetchWithTimeout(ms: number) {
  return (uri, options) => {
    let timeout;
    const timeoutP = new Promise((_, reject) => {
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
