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

import { GQLDetailedRetentionScript, GQLPlugin, GQLRetentionScript } from 'app/types/schema';
import * as mockIconSvg from 'assets/images/icons/query.svg';

export function getMockPluginsForRetentionScripts(): Pick<GQLPlugin, 'id' | 'name' | 'description' | 'logo'>[] {
  return [
    {
      id: 'foo-plugin',
      name: 'Foo Plugin',
      description: 'Description for Foo Plugin',
      logo: decodeURIComponent((mockIconSvg as string).substring(19)),
    },
    {
      id: 'bar-plugin',
      name: 'Bar Plugin With No Config',
      description: '', // Intentionally absent to test CSS
    },
    {
      id: 'baz-plugin',
      name: 'Baz has a very long name for a plugin, just to test if it overflows its container',
      description: 'Extra long description for Baz Plugin, which was made by some company that probably has an extra'
      + ' long pedigree that is worthy of overflowing text containers everywhere its name is mentioned. Seriously,'
      + ' this description is longer than any realistic one just to make sure overflow trimming works. Does it?',
    },
  ];
}

export function getMockRetentionScripts(): GQLRetentionScript[] {
  return [
    {
      id: 'foo-script',
      name: 'Foo Script',
      description: 'I have a description! It is kind of long and might overflow the container. Does it do so?',
      frequencyS: 300, // 5 minutes
      enabled: true,
      clusters: ['foo-cluster', 'bar-cluster', 'baz-cluster'],
      pluginID: 'foo-plugin',
      isPreset: true,
    },
    {
      id: 'foo-two-script',
      name: 'Foo Two',
      description: 'Two of House Foo, Second to the Throne, Future King and/or Queen of Their Bifurcated Kingdom',
      frequencyS: 59,
      enabled: false,
      clusters: [],
      pluginID: 'foo-plugin',
      isPreset: true,
    },
    {
      id: 'baz-script',
      name: 'Baz Script',
      description: 'Baz script...',
      frequencyS: 300,
      enabled: true,
      clusters: ['baz-cluster'],
      pluginID: 'baz-plugin',
      isPreset: true,
    },
    {
      id: 'custom-script',
      name: 'A custom script',
      description: 'It is a custom script. It does something custom.',
      frequencyS: 60 * 60 * 24,
      enabled: false,
      clusters: [
        'foo-cluster', 'bar-cluster', 'baz-cluster',
        'foobar-cluster', 'foobaz-cluster', 'barbaz-cluster',
        'bazbar-cluster', 'bazfoo-cluster', 'barfoo-cluster',
        'foobarbaz-cluster', 'foobazbar-cluster',
        'barfoobaz-cluster', 'barbazfoo-cluster',
        'bazfoobar-cluster', 'bazbarfoo-cluster',
      ],
      pluginID: 'bar-plugin',
      isPreset: false,
    },
  ];
}

export function getMockRetentionScriptDetails(id: string): GQLDetailedRetentionScript {
  const all = getMockRetentionScripts();
  const found = all.find(p => p.id === id);
  if (!found) throw new Error(`Plugin not in mock data: ${id}`);
  return {
    ...found,
    contents: 'I am a fake script',
    customExportURL: '',
  };
}
