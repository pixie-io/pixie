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

import { USER_QUERIES } from '@pixie-labs/api';
import { ApolloError } from '@apollo/client';
import { useOrgUsers } from './use-org-users';
import { itPassesBasicHookTests } from '../testing/hook-testing-boilerplate';

describe('useOrgUsers hook for user profile data', () => {
  const good = [
    {
      request: {
        query: USER_QUERIES.GET_ORG_USERS,
      },
      result: {
        data: {
          orgUsers: [{
            id: 'jdoe',
            email: 'jdoe@example.com',
            name: 'Jamie Doe',
            picture: '',
            orgName: 'ExampleCorp',
            orgID: '00112233-4455-6677-8899-aabbccddeeff',
          }],
        },
      },
    },
  ];

  const bad = [{
    request: { query: USER_QUERIES.GET_ORG_USERS },
    error: new ApolloError({ errorMessage: 'Request failed!' }),
  }];

  itPassesBasicHookTests({
    happyMocks: good,
    sadMocks: bad,
    useHookUnderTest: () => {
      const [orgUsers, loading, error] = useOrgUsers();
      return { payload: orgUsers, loading, error };
    },
    getPayloadFromMock: (mock) => (mock as typeof good[0]).result.data.orgUsers,
  });
});
