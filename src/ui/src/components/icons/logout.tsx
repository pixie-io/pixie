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

import { SvgIcon, SvgIconProps } from '@mui/material';

// eslint-disable-next-line react-memo/require-memo,react/display-name
export const LogoutIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M17 26H7V8H17V6H7C5.9 6 5 6.9 5 8V26C5 27.11 5.9 28 7 28H17V26Z
       `}
    />
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M24.1716 15.5L19.8788 11.2072L21.293 9.79297L28 16.4999L21.293
        23.2069L19.8788 21.7927L24.1715 17.5H12.086V15.5H24.1716Z
       `}
    />
  </SvgIcon>
);
