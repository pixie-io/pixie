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

import Axios from 'axios';

import { GetCSRFCookie } from 'app/pages/auth/utils';
import pixieAnalytics from 'app/utils/analytics';
import * as RedirectUtils from 'app/utils/redirect-utils';

import { BasePage } from './base';

// eslint-disable-next-line react-memo/require-memo
export const LogoutPage: React.FC = () => {
  // eslint-disable-next-line class-methods-use-this
  React.useEffect(() => {
    Axios.post('/api/auth/logout', {}, { headers: { 'x-csrf': GetCSRFCookie() } }).then(() => {
      try {
        localStorage.clear();
        sessionStorage.clear();
        pixieAnalytics.reset();
      } catch {
        // When embedded, referencing localStorage can throw if user settings are strict enough.
        // Similarly, some browsers block sessionStorage when in private/incognito/etc tabs.
      }
      RedirectUtils.redirect('', {});
    });
  }, []);

  return (
    <BasePage>
      Logging out...
    </BasePage>
  );
};
LogoutPage.displayName = 'LogoutPage';
