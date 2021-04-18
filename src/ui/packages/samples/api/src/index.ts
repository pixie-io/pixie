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

import { PixieAPIClient } from '@pixie-labs/api';
import chalk from 'chalk';
import demonstrateWith from './demonstrate';

async function main() {
  const apiKey = process.env.PIXIE_API_KEY;
  if (!apiKey) {
    console.log(chalk.red`This demo requires the environment variable ${chalk.bold('PIXIE_API_KEY')} to be set.`);
    process.exit(1);
  }
  console.log(chalk.white.bold`Starting API client demonstration`);

  const client = await PixieAPIClient.create({
    apiKey,
  });

  demonstrateWith(client).catch((reason) => {
    console.error(reason);
    process.exit(1);
  });
}

// This file was run directly by Node, rather than imported by another module.
try {
  if (require.main === module) {
    main().then();
  } else {
    console.error('This file is meant to be run directly, not imported.');
    process.exit(1);
  }
} catch (e) {
  console.error(chalk.red`Something broke in the bundling process: ${chalk.bold(e.message)}`);
  process.exit(1);
}
