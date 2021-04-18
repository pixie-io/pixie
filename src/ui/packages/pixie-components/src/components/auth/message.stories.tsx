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

import { FrameElement } from 'utils/frame-utils';
import { AuthMessageBox } from './message';

export default {
  title: 'Auth/Message Box',
  component: AuthMessageBox,
  decorators: [
    (Story) => (
      <FrameElement width={500}>
        <Story />
      </FrameElement>
    ),
  ],
};

export const Completed = () => (
  <AuthMessageBox
    title='Auth Completed'
    message='Please close this window and return to the CLI.'
  />
);

export const Error = () => (
  <AuthMessageBox
    error='recoverable'
    title='Auth Failed'
    message='Check your spelling and try again.'
  />
);

export const ErrorDetails = () => (
  <AuthMessageBox
    error='fatal'
    errorDetails='Internal error: bad things happened'
    title='Auth Failed'
    message='Login to this org is not allowed.'
  />
);

export const Code = () => (
  <AuthMessageBox
    title='Code Box'
    message='Please copy and paste this code!'
    code='a9123sd12321asda-sd123213as-as12'
  />
);
