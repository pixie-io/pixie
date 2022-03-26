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

import { PixieAPIClient, PixieAPIContext } from 'app/api';
import { GQLPlugin, GQLPluginConfig, GQLPluginInfo } from 'app/types/schema';
import * as mockIconSvg from 'assets/images/icons/query.svg';

export function getMockRetentionPlugins(): Array<GQLPlugin & { __typename: string }> {
  return [
    {
      name: 'Recursive Plugin',
      id: 'recursive-plugin',
      description: 'Mock data: fully specified and enabled retention plugin',
      logo: decodeURIComponent((mockIconSvg as string).substring(19)),
      latestVersion: '0.0.1-alpha',
      supportsRetention: true,
      retentionEnabled: true,
      enabledVersion: '0.0.1-alpha',
    },
    {
      name: 'Foo Plugin By Long-Named Company Registered Trademark',
      id: 'foo-plugin',
      description: 'Mock data: enabled minimal retention. Also, this description is quite long.'
        + '\nIt even has a newline! Is that not interesting?',
      latestVersion: '0.0.1-alpha',
      supportsRetention: true,
      retentionEnabled: true,
    },
    {
      name: 'Bar Plugin',
      id: 'bar-plugin',
      description: 'Mock data: disabled minimal retention',
      logo: 'Unexpected and possibly malformed or malicious logo!',
      latestVersion: '0.0.1-alpha',
      supportsRetention: true,
      retentionEnabled: false,
    },
    {
      name: 'Bad Plugin',
      id: 'bad-plugin',
      description: 'Mock data: not a retention plugin, missing config data, generally corrupted.',
      latestVersion: '0.0.1-alpha',
      supportsRetention: false,
      retentionEnabled: false,
    },
  ].map(d => ({ __typename: 'Plugin', ...d }));
}

const mockSchemas: Record<string, GQLPluginInfo['configs']> = {
  'recursive-plugin': [
    { name: 'API Key', description: 'Place your API key here, duh.' },
    { name: 'Batch Rate (Minutes)', description: 'How often should exporting happen?' },
    { name: 'Favorite Color', description: 'You know what this does.' },
  ],
  'foo-plugin': [
    { name: 'First Arg', description: '1a' },
    { name: 'Second Arg', description: '2b' },
    { name: 'Third Arg', description: '3c' },
  ],
  'bar-plugin': [
    { name: 'First Arg', description: 'Baz 1' },
    { name: 'Second Arg', description: 'Baz 2' },
    { name: 'Third Arg', description: 'Baz 3' },
  ],
};

export function getMockSchemaFor(id: string): (GQLPluginInfo & { __typename: string }) | null {
  return mockSchemas[id] ? {
    __typename: 'PluginInfo',
    configs: mockSchemas[id].map(c => ({ __typename: 'PluginConfigSchema', ... c })),
  } : null;
}

const mockDefaultConfigs: Record<string, GQLPluginConfig[]> = {
  'recursive-plugin': [
    { name: 'API Key', value: 'abcd-1234-secr-etiv-e111' },
    { name: 'Batch Rate (Minutes)', value: '5' },
    { name: 'Favorite Color', value: 'Probably something in the visible spectrum' },
  ],
  'foo-plugin': [
    { name: 'First Arg', value: '1a Value' },
    { name: 'Second Arg', value: '2b Value' },
    { name: 'Third Arg', value: '3c Value' },
  ],
  'bar-plugin': [/* No default values pre-set */],
};

export function getMockDefaultConfigFor(id: string): GQLPluginConfig[] | null {
  return mockDefaultConfigs[id] ? (
    mockDefaultConfigs[id].map(c => ({ __typename: 'PluginConfig', ... c }))
  ) : null;
}

export function useApolloCacheForMock() {
  const client = React.useContext(PixieAPIContext);
  return (client as PixieAPIClient).getCloudClient().graphQL.cache;
}
