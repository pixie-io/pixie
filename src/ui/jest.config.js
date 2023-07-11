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

const esModules = ['@mui/material', '@babel/runtime/helpers/esm', 'd3-interpolate', 'd3-color'].join('|');

module.exports = {
  testEnvironment: 'jsdom',
  setupFilesAfterEnv: [
    '<rootDir>/src/testing/jest-test-setup.js',
  ],
  moduleFileExtensions: [
    'js',
    'json',
    'jsx',
    'mjs',
    'ts',
    'tsx',
  ],
  moduleDirectories: [
    '<rootDir>/src',
  ],
  moduleNameMapper: {
    '^.+.(jpg|jpeg|png|gif|svg)$': '<rootDir>/src/testing/file-mock.js',
    'typeface-walter-turncoat': '<rootDir>/src/testing/style-mock.js',
    'monaco-editor': require.resolve('react-monaco-editor'),
    '^configurable/(.*)': '<rootDir>/src/configurables/base/$1',
    '^app/(.*)': '<rootDir>/src/$1',
  },
  resolver: null,
  transform: {
    ...['ts', 'tsx', 'js', 'jsx'].reduce((a, ext) => ({
      ...a,
      [`^.+\\.${ext}$`]: ['esbuild-jest', {
        loader: ext,
        sourcemap: true,
        target: 'node16',
      }]
    }), {}),
    [`node_modules/(${esModules}).*\\.jsx?$`]: './jest-esm-transform',
    '^.+\\.toml$': 'jest-raw-loader',
  },
  testRegex: '.*test\\.(ts|tsx|js|jsx)$',
  reporters: [
    'default',
    'jest-junit',
  ],
  // We need to specify the inverse of the esModules above, to make sure
  // we don't use the default (ignore all node modules).
  transformIgnorePatterns: [`/node_modules/(?!${esModules})`],
  collectCoverageFrom: [
    'src/**/*.ts',
    'src/**/*.tsx',
    'src/**/*.js',
    'src/**/*.jsx',
    'src/*.ts',
    'src/*.tsx',
    'src/*.js',
    'src/*.jsx',
    '!src/types/generated/**/*',
  ],
};
