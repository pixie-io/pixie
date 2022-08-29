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

import { createBrowserHistory } from 'history';

import { isPixieEmbedded } from 'app/common/embed-context';
import pixieAnalytics from 'app/utils/analytics';

const history = createBrowserHistory();

function allowIntercomDefaultLauncher(path: string): boolean {
  const isEmbedded = isPixieEmbedded();
  const allowedPaths = ['/auth/login', '/auth/signup'];

  return !isEmbedded && allowedPaths.includes(path);
}

function sendPageEvent(path: string, search: string) {
  // Log the event to our analytics (if enabled).
  pixieAnalytics.page(
    '', // category
    path, // name
    {}, // properties
    {
      integrations: {
        Intercom: { hideDefaultLauncher: !allowIntercomDefaultLauncher(path) },
      },
    }, // options
  );

  // If embedded, also allow the parent to track the event by
  // sending it a message.
  if (isPixieEmbedded()) {
    window.top.postMessage({ pixieURLChange: `${path}${search}` }, '*');
  }
}

// Emit a page event for the first loaded page.
sendPageEvent(window.location.pathname, window.location.search);

history.listen((location) => {
  sendPageEvent(location.pathname, window.location.search);
});

export default history;
