import * as QueryString from 'query-string';

export function setQueryParams(params: { [key: string]: string }) {
  const { protocol, host, pathname } = window.location;
  const newQueryString = QueryString.stringify(params);
  const search = newQueryString ? `?${newQueryString}` : '';
  const newurl = `${protocol}//${host}${pathname}${search}`;

  window.history.pushState({ path: newurl }, '', newurl);
}

export function getQueryParams(): { [key: string]: string } {
  const params = {};
  const parsed = QueryString.parse(location.search);
  for (const key of Object.keys(parsed)) {
    if (typeof parsed[key] === 'string') {
      params[key] = parsed[key];
    }
  }
  return params;
}
