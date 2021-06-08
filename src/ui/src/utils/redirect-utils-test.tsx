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

import * as RedirectUtils from './redirect-utils';

jest.mock('app/containers/constants', () => ({ DOMAIN_NAME: 'dev.withpixie.dev' }));

describe('RedirectUtils test', () => {
  it('should return correct url for no params', () => {
    expect(RedirectUtils.getRedirectPath('/vizier/query', {})).toEqual(
      'http://dev.withpixie.dev/vizier/query',
    );
  });

  it('should return correct url for params', () => {
    expect(RedirectUtils.getRedirectPath('/vizier/query', { test: 'abc', param: 'def' })).toEqual(
      'http://dev.withpixie.dev/vizier/query?test=abc&param=def',
    );
  });
});
