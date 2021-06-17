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
import {
  parseVis, Vis,
} from 'app/containers/live/vis';

const PROD_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json';
const OSS_SCRIPTS = 'https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-oss.json';

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

export function GetPxScripts(orgID: string, orgName: string): Promise<Script[]> {
  let localStorageCoreBundle = localStorage.getItem('px-custom-core-bundle-path');
  let localStorageOSSBundle = localStorage.getItem('px-custom-oss-bundle-path');
  if (localStorageCoreBundle) {
    localStorageCoreBundle = bypassCacheURL(localStorageCoreBundle);
  }
  if (localStorageOSSBundle) {
    localStorageOSSBundle = bypassCacheURL(localStorageOSSBundle);
  }
  const coreBundlePath = localStorageCoreBundle || PROD_SCRIPTS;
  const ossBundlePath = localStorageOSSBundle || OSS_SCRIPTS;
  const fetchPromises = [Axios({ method: 'get', url: coreBundlePath }), Axios({ method: 'get', url: ossBundlePath })];
  return Promise.all(fetchPromises).then((response) => {
    const scripts = [];
    response.forEach((resp) => {
      Object.entries(resp.data.scripts as ScriptJSON[]).forEach(([id, s]) => {
        if (s.orgID && orgID !== s.orgID) {
          return;
        }
        let prettyID = id;
        if (id.startsWith('org_id/')) {
          if (!orgName) {
            return;
          }
          const splits = id.split('/', 3);
          if (splits.length < 3) {
            return;
          }
          if (splits[1] !== orgID) {
            return;
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
      });
    });
    return scripts;
  });
}
