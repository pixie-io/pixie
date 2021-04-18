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
