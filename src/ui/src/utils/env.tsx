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

import { DOMAIN_NAME, SEGMENT_UI_WRITE_KEY } from 'app/containers/constants';
import { format } from 'date-fns';

// Webpack's EnvPlugin has trouble understanding destructuring when Babel gets to it first.
// This is the case in our build (by necessity), so we must write this long-form.
/* eslint-disable prefer-destructuring */
const STABLE_BUILD_NUMBER = process.env.STABLE_BUILD_NUMBER;
const STABLE_BUILD_SCM_REVISION = process.env.STABLE_BUILD_SCM_REVISION;
const STABLE_BUILD_SCM_STATUS = process.env.STABLE_BUILD_SCM_STATUS;
const BUILD_TIMESTAMP = process.env.BUILD_TIMESTAMP;
/* eslint-enable prefer-destructuring */

const timestampSec = Number.parseInt(BUILD_TIMESTAMP, 10);
const date = Number.isNaN(timestampSec) ? new Date() : new Date(timestampSec * 1000);
const dateStr = format(date, 'YYYY.MM.DD.hh.mm');
const parts = [];
if (typeof STABLE_BUILD_SCM_REVISION === 'string') {
  parts.push(STABLE_BUILD_SCM_REVISION.substr(0, 7));
}
if (STABLE_BUILD_SCM_STATUS) {
  parts.push(STABLE_BUILD_SCM_STATUS);
}
parts.push(Number.isNaN(timestampSec) ? Math.floor(date.valueOf() / 1000) : timestampSec);
if (STABLE_BUILD_NUMBER) {
  parts.push(STABLE_BUILD_NUMBER);
}

export const PIXIE_CLOUD_VERSION = `${dateStr}+${parts.join('.')}`;

export function isDev(): boolean {
  return DOMAIN_NAME.startsWith('dev');
}

export function isStaging(): boolean {
  return DOMAIN_NAME.startsWith('staging');
}

export function isProd(): boolean {
  return !isDev() && !isStaging();
}

function isValidSegmentKey(k) {
  // The TS compiler is really smart and is optmizing away the checks,
  // which is why this check is so convoluted...
  return k && !k.startsWith('__S');
}

export function isValidAnalytics(): boolean {
  return isValidSegmentKey(SEGMENT_UI_WRITE_KEY);
}
