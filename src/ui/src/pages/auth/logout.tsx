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
import * as RedirectUtils from 'app/utils/redirect-utils';
import { BasePage } from './base';
import { GetCSRFCookie } from './utils';

// eslint-disable-next-line react/prefer-stateless-function
export const LogoutPage: React.FC = () => {
  // eslint-disable-next-line class-methods-use-this
  React.useEffect(() => {
    Axios.post('/api/auth/logout', {}, { headers: { 'x-csrf': GetCSRFCookie() } }).then(() => {
      localStorage.clear();
      sessionStorage.clear();
      analytics.reset();
      RedirectUtils.redirect('', {});
    });
  }, []);

  return (
    <BasePage>
      Logging out...
    </BasePage>
  );
};
