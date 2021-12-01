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
export const EditIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 21 21'>
    <path d='M20.408 1.70718L18.9938 0.292969L7.29297 11.9938L8.70718 13.408L20.408 1.70718Z' />
    <path
      d={`M2 5.00012V19.0001H16V12.0001H18V19.0001C18 20.1001 17.1 21.0001 16 21.0001H2C0.89 21.0001 0 20.1001 0
    19.0001V5.00012C0 3.90012 0.89 3.00012 2 3.00012H9V5.00012H2Z`}
    />
  </SvgIcon>
);
