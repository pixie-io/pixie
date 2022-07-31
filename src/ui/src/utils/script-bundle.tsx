/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import Axios from 'axios';
import * as QueryString from 'query-string';

import { SCRIPT_BUNDLE_DEV, SCRIPT_BUNDLE_URLS } from 'app/containers/constants';
import {
  parseVis, Vis,
} from 'app/containers/live/vis';

const OVERRIDE_URLS_KEY = 'px-custom-script-bundle-paths';
const OLD_OVERRIDE_CORE_KEY = 'px-custom-core-bundle-path';
const OLD_OVERRIDE_OSS_KEY = 'px-custom-oss-bundle-path';

export interface Script {
  id: string;
  title: string;
  code: string;
  vis: Vis;
  description?: string;
  hidden?: boolean;
}

interface ScriptJSON {
  ShortDoc: string;
  pxl: string;
  vis: string;
  LongDoc: string;
  hidden: boolean;
  orgID: string;
}

// bypassCacheURL adds a timestamp to the URL to bypass the disk-cache.
// Necessary because disabling cache in headers doesn't work.
// https://stackoverflow.com/questions/49263559/using-javascript-axios-fetch-can-you-disable-browser-cache
function bypassCacheURL(url: string) {
  const queryURL = QueryString.parseUrl(url);
  queryURL.query.timestamp = `${new Date().getTime()}`;
  return QueryString.stringifyUrl(queryURL);
}

function getBundleUrls(): { urls: string[], isDev: boolean } {
  let urls: string[] = JSON.parse(SCRIPT_BUNDLE_URLS); // Should be a string[] in JSON form.
  let isDev = SCRIPT_BUNDLE_DEV;

  // Backwards compatibility: honor older localStorage keys if present. Newer key overrides.
  try {
    const localCore = localStorage.getItem(OLD_OVERRIDE_CORE_KEY);
    const localOss = localStorage.getItem(OLD_OVERRIDE_OSS_KEY);
    if (localCore || localOss) {
      urls = [localCore, localOss].filter(u => u);
      isDev = true;
    }
  } catch { /* localStorage isn't guaranteed to be available */}

  try {
    const localRaw = localStorage.getItem(OVERRIDE_URLS_KEY);
    const localParsed = JSON.parse(localRaw);
    if (Array.isArray(localParsed) && localParsed.length > 0) {
      urls = localParsed;
      isDev = true;
    }
  } catch { /* localStorage isn't guaranteed to be available; even if it is, the setting need to be valid. */ }

  return {
    urls: isDev ? urls.map((url) => bypassCacheURL(url)) : urls,
    isDev,
  };
}

export function GetPxScripts(orgID: string, orgName: string): Promise<Script[]> {
  const { urls, isDev } = getBundleUrls();
  const fetchPromises = urls.map(url => Axios({ method: 'get', url }));
  return Promise.all(fetchPromises)
    .then((response) => {
      const scripts: Script[] = [];
      for (const resp of response) {
        for (const [id, s] of Object.entries<ScriptJSON>(resp.data.scripts)) {
          if (s.orgID && orgID !== s.orgID) {
            continue;
          }
          let prettyID = id;
          if (id.startsWith('org_id/')) {
            if (!orgName) {
              continue;
            }
            const splits = id.split('/', 3);
            if (splits.length < 3) {
              continue;
            }
            if (splits[1] !== orgID) {
              continue;
            }
            prettyID = `${orgName}/${splits[2]}`;
          }
          let vis = {
            variables: [],
            widgets: [],
            globalFuncs: [],
          } as Vis;
          try {
            vis = parseVis(s.vis);
          } catch (e) {
            return;
          }
          scripts.push({
            id: prettyID,
            title: s.ShortDoc,
            code: s.pxl,
            vis,
            description: s.LongDoc,
            hidden: s.hidden,
          });
        }
      }
      return scripts;
    })
    .catch((error) => {
      if (isDev) {
        error.message = `${error.message} (SCRIPT_BUNDLES_OVERRIDDEN)`;
      }
      throw error;
    });
}
