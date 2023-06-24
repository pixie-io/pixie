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

import { format } from 'date-fns';

import { SEGMENT_UI_WRITE_KEY } from 'app/containers/constants';

// Webpack's EnvPlugin has trouble understanding destructuring when Babel gets to it first.
// This is the case in our build (by necessity), so we must write this long-form.
/* eslint-disable prefer-destructuring */
const STABLE_BUILD_TAG = process.env.STABLE_BUILD_TAG;
const BUILD_TIMESTAMP = process.env.BUILD_TIMESTAMP;
/* eslint-enable prefer-destructuring */

const timestampSec = Number.parseInt(BUILD_TIMESTAMP, 10);
const dateStr = Number.isNaN(timestampSec) ? 'time unknown' : format(timestampSec * 1000, 'yyyy.MM.dd.hh.mm');

export const PIXIE_CLOUD_VERSION = {
  date: dateStr,
  tag: STABLE_BUILD_TAG,
};

function isValidSegmentKey(k) {
  // The TS compiler is really smart and is optmizing away the checks,
  // which is why this check is so convoluted...
  return k && !k.startsWith('__S');
}

export function isValidAnalytics(): boolean {
  return isValidSegmentKey(SEGMENT_UI_WRITE_KEY);
}
