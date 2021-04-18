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

import * as React from 'react';

import { StatusCell } from 'components/status/status';
import { Breadcrumbs } from './breadcrumbs';

export default {
  title: 'Breadcrumbs',
  component: Breadcrumbs,
  subcomponents: { StatusCell },
  decorators: [
    (Story) => (
      <div style={{ backgroundColor: '#212324' }}>
        <Story />
      </div>
    ),
  ],
};

export const Basic = () => {
  const breadcrumbs = [
    {
      title: 'cluster',
      value: 'gke-prod',
      selectable: true,
      allowTyping: false,
      // eslint-disable-next-line
      getListItems: async () => [
        // Description is optional, so only providing it on some to demonstrate what happens when it isn't provided.
        {
          value: 'cluster1',
          icon: <StatusCell statusGroup='healthy' />,
          description: 'Cluster 1 description',
        },
        {
          value: 'cluster2',
          icon: <StatusCell statusGroup='unhealthy' />,
          description: 'Cluster 2 description',
        },
        { value: 'cluster3', icon: <StatusCell statusGroup='pending' /> },
      ],
      onSelect: (input) => {
        // eslint-disable-next-line
        console.log(`Selected cluster: ${input}`);
      },
    },
    {
      title: 'pod',
      value: 'pod-123',
      selectable: true,
      allowTyping: true,
      getListItems: async (input) => {
        if (input === '') {
          return [
            { value: 'pod1' },
            {
              value: 'pod2',
              description: 'Pod 2 has a description, 1 does not',
            },
          ];
        }
        return [
          { value: 'some pod' },
          { value: 'another pod', description: 'Interrupting cow says what?' },
          { value: 'pod' },
        ];
      },
      onSelect: (input) => {
        // eslint-disable-next-line
        console.log(`Selected pod: ${input}`);
      },
    },
    {
      title: 'script',
      value: 'px/pod',
      selectable: false,
    },
  ];

  return <Breadcrumbs breadcrumbs={breadcrumbs} />;
};
