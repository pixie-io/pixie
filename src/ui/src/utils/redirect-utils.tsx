import {DOMAIN_NAME} from 'containers/constants';
import * as _ from 'lodash';

interface StringMap {
  [s: string]: string;
}

export function redirect(path: string, params: StringMap) {
  window.location.href = getRedirectPath(path, params);
}

export function getRedirectPath(path: string, params: StringMap) {
  const port = window.location.port ? ':' + window.location.port : '';
  let queryParams = '';

  const paramKeys = Object.keys(params);
  if (paramKeys.length > 0) {
    const paramStrings = _.map(paramKeys, (key) => {
      return key + '=' + params[key];
    });
    queryParams = '?' + paramStrings.join('&');
  }

  return window.location.protocol + '//' + DOMAIN_NAME + port + path + queryParams;
}
