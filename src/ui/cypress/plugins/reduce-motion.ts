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

/**
 * Tell browsers to reduce motion, which also reduces how long some tests have to wait
 * for elements to change states, appear/disappear, etc.
 */
export const reduceMotionPlugin: Cypress.PluginConfig = (on) => {
  on('before:browser:launch', (browser, launchOptions) => {
    if (browser.family === 'chromium') {
      if (browser.name === 'electron') {
        // Electron doesn't seem to have a setting for this.
        // https://www.electronjs.org/docs/latest/api/browser-window#new-browserwindowoptions
        // launchOptions.preferences.SOMETHING = true;
      } else {
        launchOptions.args.push('--force-prefers-reduced-motion');
      }
    } else if (browser.family === 'firefox') {
      launchOptions.preferences['ui.prefersReducedMotion'] = 1;
    }
    return launchOptions;
  });
};
