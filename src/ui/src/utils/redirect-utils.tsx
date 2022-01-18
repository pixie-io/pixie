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

import { DOMAIN_NAME } from 'app/containers/constants';

export function getRedirectPath(path: string, params: Record<string, string> = {}): string {
  const port = window.location.port ? `:${window.location.port}` : '';
  let queryParams = '';

  const paramKeys = Object.keys(params);
  if (paramKeys.length > 0) {
    const paramStrings = paramKeys.map((key) => `${key}=${params[key]}`);
    queryParams = `?${paramStrings.join('&')}`;
  }

  return `${window.location.protocol}//${DOMAIN_NAME}${port}${path}${queryParams}`;
}

export function redirect(path: string, params: Record<string, string> = {}): void {
  window.location.href = getRedirectPath(path, params);
}
