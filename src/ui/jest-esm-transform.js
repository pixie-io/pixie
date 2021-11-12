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

// This file calls Babel manually on js(x) files.
// This is necessary because we have esm node_modules
// and they are ignored by default. Unfortunately, changing
// the tranform views is not enough since some Babel config is
// preventing the transformation without this.

const babelJest = require('babel-jest').default.createTransformer(require('./.babelrc.json'));

module.exports = {
  process: function(src, file, config, transformOptions) {
    if (file.match(/.*jsx?$/))  {
      return babelJest.process.bind(this)(src, file, config, transformOptions);
    }
    console.warn('Unknown extension for file (passing through):', file);
    return src;
  },
};
