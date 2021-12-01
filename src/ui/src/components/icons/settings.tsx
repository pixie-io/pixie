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
export const SettingsIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 24 24'>
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M19.4301 12.98C19.4701 12.66 19.5001 12.34 19.5001 12C19.5001 11.66 19.4701 11.34
        19.4301 11.02L21.5401 9.37C21.7301 9.22 21.7801 8.95 21.6601 8.73L19.6601 5.27C19.5401
        5.05 19.2701 4.97 19.0501 5.05L16.5601 6.05C16.0401 5.65 15.4801 5.32 14.8701 5.07L14.4901
        2.42C14.4601 2.18 14.2501 2 14.0001 2H10.0001C9.75008 2 9.54008 2.18 9.51008 2.42L9.13008
        5.07C8.52008 5.32 7.96008 5.66 7.44008 6.05L4.95008 5.05C4.72008 4.96 4.46008 5.05 4.34008
        5.27L2.34008 8.73C2.21008 8.95 2.27008 9.22 2.46008 9.37L4.57008 11.02C4.53008 11.34
        4.50008 11.67 4.50008 12C4.50008 12.33 4.53008 12.66 4.57008 12.98L2.46008 14.63C2.27008
        14.78 2.22008 15.05 2.34008 15.27L4.34008 18.73C4.46008 18.95 4.73008 19.03 4.95008
        18.95L7.44008 17.95C7.96008 18.35 8.52008 18.68 9.13008 18.93L9.51008 21.58C9.54008 21.82
        9.75008 22 10.0001 22H14.0001C14.2501 22 14.4601 21.82 14.4901 21.58L14.8701 18.93C15.4801
        18.68 16.0401 18.34 16.5601 17.95L19.0501 18.95C19.2801 19.04 19.5401 18.95 19.6601
        18.73L21.6601 15.27C21.7801 15.05 21.7301 14.78 21.5401 14.63L19.4301 12.98ZM12.0001
        15.5C10.0701 15.5 8.50008 13.93 8.50008 12C8.50008 10.07 10.0701 8.5 12.0001 8.5C13.9301
        8.5 15.5001 10.07 15.5001 12C15.5001 13.93 13.9301 15.5 12.0001 15.5Z
    `}
    />
  </SvgIcon>
);
