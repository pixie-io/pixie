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
export const MagicIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 17.91 19.38'>
    <path d='M6 3V5H7V3H9V2H7V0H6V2H4V3H6Z' />
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M17.3807 5.41424L14.5522 2.58582L0.585449 16.5525L3.41388 19.3809L17.3807 5.41424ZM13.4969
        7.88381L12.0827 6.46959L14.5522 4.00003L15.9664 5.41424L13.4969 7.88381Z`}
    />
    <path d='M17 13.5V12.5H16V12H17V11H17.5V12H18.5V12.5H17.5V13.5H17Z' />
  </SvgIcon>
);
