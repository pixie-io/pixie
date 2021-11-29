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

import { GQLAPIKeyMetadata } from 'app/types/schema';

import { formatAPIKey } from './api-keys';

describe('formatApiKey', () => {
  it('correctly formats API keys', () => {
    const apiKeyResults: GQLAPIKeyMetadata[] = [
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        createdAtMs: new Date(new Date().getTime() - 1000 * 60 * 60 * 24 * 2).getTime(), // 2 days ago
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        createdAtMs: new Date(new Date().getTime() - 1000 * 60 * 60 * 3).getTime(), // 3 hours ago
        desc: 'abcd2',
      },
    ];
    expect(apiKeyResults.map((key) => formatAPIKey(key))).toStrictEqual([
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        createdAt: '2 days ago',
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        createdAt: 'about 3 hours ago',
        desc: 'abcd2',
      },
    ]);
  });
});
