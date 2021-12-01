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
export const PodIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      d={`M5.00023 8.21597L16.0902 5L27.1802 8.21597L16.0902 11.4319L5.00023
        8.21597ZM5 9.44775V21.2482L15.3328 26.9718L15.3839 12.5361L5
        9.44775ZM27.1808 21.2482V9.44775L16.7969 12.5361L16.848 26.9718L27.1808 21.2482Z`}
    />
  </SvgIcon>
);
