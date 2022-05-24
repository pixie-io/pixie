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

const fs = require('fs');
const path = require('path');

const [, scriptPath, ...args] = process.argv;

const input = (args.find((s) => s.startsWith('--input=')) || '').split('=')[1];
const output = (args.find((s) => s.startsWith('--output=')) || '').split('=')[1];

if (!input || !output) {
  const scriptFilename = scriptPath.slice(scriptPath.lastIndexOf('/'));
  console.error(`Usage: ${scriptFilename} --input=/foo/in.json --output=/bar/out.json`);
  process.exit(1);
}

/*
 * Input example (from `yarn license_check --excludePrivatePackages --production --json --out /foo/.../licenses.json`):
 *
 * 'vega-util@1.16.1': {
 *   licenses: 'BSD-3-Clause',
 *   repository: 'https://github.com/vega/vega',
 *   publisher: 'Jeffrey Heer',
 *   url: 'http://idl.cs.washington.edu',
 *   path: '/Users/nlanam/Documents/Development/pixielabs/src/ui/node_modules/vega-util',
 *   licenseFile: '/Users/nlanam/Documents/Development/pixielabs/src/ui/node_modules/vega-util/LICENSE'
 * }
 *
 * Output format (defined in, and consumed by, //src/ui/src/pages/credits/credits.tsx):
 *
 * interface LicenseEntry {
 *   name: string;
 *   spdxID?: string;
 *   url?: string;
 *   licenseText?: string;
 * }
 */

const GITHUB_MATCHER = /github.com\/([^/]+)\/([^/]+)/;
const LICENSE_PREFERENCE = {
  'CC0-1.0': 10,
  'Apache-2.0': 9,
  'MIT': 8,
};

function getName(projectName, licenseDetails) {
  if (licenseDetails.repository) {
    const [,org,proj] = licenseDetails.repository.match(GITHUB_MATCHER);
    if (org && proj) return `${org}/${proj}`;
  }

  if (licenseDetails.path) {
    const split = licenseDetails.path.split('/node_modules/');
    if (split && split.length > 0) return split[1];
  }

  return projectName.split('@')[0];
}

function getCleanRepo(projectName, licenseDetails) {
  if (!licenseDetails.repository) return '';

  const [,org,proj] = licenseDetails.repository.match(GITHUB_MATCHER);
  return org && proj ? `https://github.com/${org}/${proj}` : undefined;
}

function getVersion(projectName) {
  const splits = projectName.split('@');
  return splits[splits.length - 1];
}

function getSpdxId(projectName, licenseDetails) {
  if (!licenseDetails.licenses) return '';

  const options = licenseDetails.licenses
    .replace('AND', 'and')
    .replace('OR', 'or')
    .split(' or ')
    .map((opt) => opt.replace('(', '').replace(')', ''));

  if (options.length === 1) return options[0];

  const best = options.sort((a, b) => {
    const aScore = LICENSE_PREFERENCE[a] || 0;
    const bScore = LICENSE_PREFERENCE[b] || 0;
    return bScore - aScore;
  })[0];

  if (!LICENSE_PREFERENCE[best]) return options.join(' or ');
  return best;
}

function getLicenseText(projectName, licenseDetails) {
  if (!licenseDetails.licenseFile) return '';
  try {
    return fs.readFileSync(path.resolve(licenseDetails.licenseFile)).toString('utf-8');
  } catch(e) {
    console.error(`Could not read license file at path ${licenseDetails.licenseFile}`);
    console.error(e);
    process.exit(3);
  }
}

let jsonIn = {};
try {
  /*
   * Note: both the license-checker package and this script are run with `yarn pnpify`, which causes their attempts to
   * read `node_modules` to silently go through a virtual filesystem. This allows us to ignore PnP entirely.
   */
  jsonIn = require(path.resolve(input));
} catch (e) {
  console.error(`Failed to read license data input file "${input}":`);
  console.error(e);
  process.exit(2);
}

const out = new Map();
for (const [key, val] of Object.entries(jsonIn)) {
  out.set(getName(key, val), {
    name: getName(key, val),
    url: getCleanRepo(key, val),
    version: getVersion(key),
    spdxId: getSpdxId(key, val),
    licenseText: getLicenseText(key, val),
  });
}

try {
  fs.writeFileSync(path.resolve(output), JSON.stringify([...out.values()], null, 2));
} catch(e) {
  console.error(`Failed to write results to ${output}`);
  console.error(e);
  process.exit(4);
}
