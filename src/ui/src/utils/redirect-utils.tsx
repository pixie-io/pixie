import { DOMAIN_NAME } from 'containers/constants';

interface StringMap {
  [s: string]: string;
}

export function getRedirectPath(path: string, params: StringMap) {
  const port = window.location.port ? `:${window.location.port}` : '';
  let queryParams = '';

  const paramKeys = Object.keys(params);
  if (paramKeys.length > 0) {
    const paramStrings = paramKeys.map((key) => `${key}=${params[key]}`);
    queryParams = `?${paramStrings.join('&')}`;
  }

  return `${window.location.protocol}//${DOMAIN_NAME}${port}${path}${queryParams}`;
}

export function redirect(path: string, params: StringMap) {
  window.location.href = getRedirectPath(path, params);
}
