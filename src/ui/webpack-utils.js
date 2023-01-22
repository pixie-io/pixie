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

const archiver = require('archiver');
const { execSync } = require('child_process');
const fs = require('fs');
const { dirname } = require('path');
const YAML = require('yaml');


// Reads the YAML file from the given sops file.
function readYAMLFile(filePath, isEncrypted) {
  const cleanedPath = filePath.replace(/\//g, '\\/');
  let results;
  if (isEncrypted) {
    results = execSync(`sops --decrypt ${cleanedPath}`, { stdio: ['pipe', 'pipe', 'ignore'] });
  } else {
    // Don't try to change this to `fs.readFileSync` to avoid the useless cat:
    // readFileSync can't find this path, cat can.
    results = execSync(`cat ${cleanedPath}`, { stdio: ['pipe', 'pipe', 'ignore'] });
  }
  return YAML.parse(results.toString());
}

class ArchivePlugin {
  constructor(options = {}) {
    this.options = options;
  }

  apply(compiler) {
    compiler.hooks.emit.tapAsync('ArchivePlugin', (compilation, callback) => {
      fs.mkdir(dirname(this.options.output), { recursive: true }, (err) => {
        if (err) {
          callback(err);
        }
        this.archiverStream = archiver('tar', {
          gzip: true,
        });
        this.archiverStream.pipe(fs.createWriteStream(this.options.output));
        callback();
      });
    });

    compiler.hooks.assetEmitted.tap('ArchivePlugin', (file, info) => {
      // Pick a deterministic mtime so that the output is not volatile.
      // This helps ensure that bazel can cache the ui builds as expected.
      this.archiverStream.append(info.content, { name: file, date: 1514764800 });
    });

    compiler.hooks.afterEmit.tap('ArchivePlugin', () => {
      this.archiverStream.finalize();
    });
  }
}

module.exports = {
  ArchivePlugin,
  readYAMLFile,
};
