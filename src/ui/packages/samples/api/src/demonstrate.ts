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

import type { GQLAutocompleteActionType } from '@pixie-labs/api';
import { GQLAutocompleteEntityKind, PixieAPIClient } from '@pixie-labs/api';
import chalk from 'chalk';

function head(message: string) {
  console.log(`\n${message}`);
}

function good(message: string) {
  // u+2713 ✓ check mark
  console.log(chalk.green(`\u2713 ${message}`));
}

function bad(message: string, fatallyBad = false) {
  // u+2715 ✕ cross
  console.log(chalk.red(`\u2715 ${message}`));
  if (fatallyBad) {
    process.exit(1);
  }
}

function assert(condition: boolean, goodMessage: string, badMessage: string) {
  if (condition) {
    good(goodMessage);
  } else {
    bad(badMessage);
  }
}

async function demonstrateKeys(client: PixieAPIClient) {
  // API KEYS
  head('Creating, reading, and deleting API keys');

  const apiKeyId = await client.createAPIKey();
  assert(!!apiKeyId, 'Created an API key', 'Failed to create an API key');

  const apiKeys = await client.listAPIKeys();
  const apiKey = apiKeys.find((k) => k.id === apiKeyId)?.key;
  assert(
    !!apiKey,
    'Newly-created API key appears in the list',
    'Newly-created API key does not appear in the list');

  await client.deleteAPIKey(apiKeyId);
  const updatedApiKeys = await client.listAPIKeys();
  assert(
    !updatedApiKeys.some((k) => k.id === apiKeyId),
    'Newly-created API key promptly deleted',
    'Newly-created API key still appears in the list after deleting it');

  // DEPLOYMENT KEYS
  head('Creating, reading, and deleting deployment keys');

  const deploymentKeyId = await client.createDeploymentKey();
  assert(!!deploymentKeyId, 'Created a deployment key', 'Failed to create a deployment key');

  const deploymentKeys = await client.listDeploymentKeys();
  const deploymentKey = deploymentKeys.find((k) => k.id === deploymentKeyId)?.key;
  assert(
    !!deploymentKey,
    'Newly-created deployment key appears in the list',
    'Newly-created deployment key does not appear in the list');

  await client.deleteDeploymentKey(deploymentKeyId);
  const updatedDeploymentKeys = await client.listDeploymentKeys();
  assert(
    !updatedDeploymentKeys.some((k) => k.id === deploymentKeyId),
    'Newly-created deployment key promptly deleted',
    'Newly-created deployment key still appears in the list after deleting it');
}

async function demonstrateClusterInfo(client: PixieAPIClient) {
  head('Listing available clusters and their control plane pods');

  const clusters = await client.listClusters();
  assert(clusters.length > 0, 'Found available clusters', 'Could not find any available clusters');

  const controlPlanePods = await client.getClusterControlPlanePods();
  assert(controlPlanePods.length > 0, 'Found control plane pods', 'Could not find any control plane pods');
}

async function demonstrateAutocomplete(client: PixieAPIClient) {
  head('Autocomplete suggestions');
  const clusters = await client.listClusters();
  assert(clusters.length > 0, 'Found available clusters', 'Could not find any available clusters');
  const id = clusters[0]?.id;

  const fullSuggester = client.getAutocompleteSuggester(id);
  const long = await fullSuggester('px/clu', 2, 'AAT_EDIT' as GQLAutocompleteActionType);
  const longCompletion = long.tabSuggestions.map((t) => t.suggestions.map((s) => s.name)).flat()[0];
  assert(
    longCompletion === 'px/cluster',
    'Full input autocomplete suggests appropriately',
    'Full input autocomplete did not give expected suggestions');

  const fieldSuggester = client.getAutocompleteFieldSuggester(id);
  const short = await fieldSuggester('px/clu', 'AEK_SCRIPT' as GQLAutocompleteEntityKind);
  const shortCompletion = short.map((s) => s.name).flat()[0];
  assert(
    shortCompletion === 'px/cluster',
    'Field autocomplete suggests appropriate script',
    'Field autocomplete did not suggest the expected script');
}

export default async function demonstrateWith(client: PixieAPIClient): Promise<void> {
  await demonstrateKeys(client);
  await demonstrateClusterInfo(client);
  await demonstrateAutocomplete(client);

  // Not testing setSetting as there isn't any safe "dummy" key that would not affect your real user settings.
  // If you're okay with having this demo adjust your user settings, try `setSetting('tourSeen', false)`.
  head('User settings');
  const tourSeen = await client.getSetting('tourSeen');
  assert(typeof tourSeen === 'boolean', 'Can retrieve settings', 'Cannot retrieve settings');

  // TODO(nick,PC-738): Demonstrate gRPC (health check and execute script) on clusters; list scripts when that exists.
}
