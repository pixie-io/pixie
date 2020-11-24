import Axios from 'axios';
import { isStaging } from 'utils/env';
import * as QueryString from 'query-string';

const PROD_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json';
const OSS_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-oss.json';
const STAGING_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-staging-core.json';

export interface Script {
  id: string;
  title: string;
  code: string;
  vis?: string;
  description?: string;
  hidden?: boolean;
}

interface ScriptJSON {
  ShortDoc: string;
  pxl: string;
  vis: string;
  LongDoc: string;
  hidden: boolean;
  orgName: string;
}

// bypassCacheURL adds a timestamp to the URL to bypass the disk-cache.
// Necessary because disabling cache in headers doesn't work.
// https://stackoverflow.com/questions/49263559/using-javascript-axios-fetch-can-you-disable-browser-cache
function bypassCacheURL(url: string) {
  const queryURL = QueryString.parseUrl(url);
  queryURL.query.timestamp = `${new Date().getTime()}`;
  return QueryString.stringifyUrl(queryURL);
}

export function GetPxScripts(orgName: string): Promise<Script[]> {
  let localStorageCoreBundle = localStorage.getItem('px-custom-core-bundle-path');
  let localStorageOSSBundle = localStorage.getItem('px-custom-oss-bundle-path');
  if (localStorageCoreBundle) {
    localStorageCoreBundle = bypassCacheURL(localStorageCoreBundle);
  }
  if (localStorageOSSBundle) {
    localStorageOSSBundle = bypassCacheURL(localStorageOSSBundle);
  }
  const coreBundlePath = localStorageCoreBundle || (isStaging() ? STAGING_SCRIPTS : PROD_SCRIPTS);
  const ossBundlePath = localStorageOSSBundle || OSS_SCRIPTS;
  const fetchPromises = [Axios({ method: 'get', url: coreBundlePath }), Axios({ method: 'get', url: ossBundlePath })];
  return Promise.all(fetchPromises).then((response) => {
    const scripts = [];
    response.forEach((resp) => {
      Object.entries(resp.data.scripts as ScriptJSON[]).forEach(([id, s]) => {
        if (s.orgName && orgName !== s.orgName) {
          return;
        }
        scripts.push({
          id,
          title: s.ShortDoc,
          code: s.pxl,
          vis: s.vis,
          description: s.LongDoc,
          hidden: s.hidden,
        });
      });
    });
    return scripts;
  });
}
