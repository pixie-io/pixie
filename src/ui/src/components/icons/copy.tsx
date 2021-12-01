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
export const CopyIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon
    {...props}
    width='32'
    height='32'
    viewBox='0 0 32 32'
    fill='none'
    xmlns='http://www.w3.org/2000/svg'
  >
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M20.336 5H8.048C6.91648 5 6 5.91648 6 7.048V21.384H8.048V7.048H20.336V5ZM23.408 9.096H12.144C11.0125
        9.096 10.096 10.0125 10.096 11.144V25.48C10.096 26.6115 11.0125 27.528 12.144 27.528H23.408C24.5395
        27.528 25.456 26.6115 25.456 25.48V11.144C25.456 10.0125 24.5395 9.096 23.408 9.096ZM12.144
        25.48H23.408V11.144H12.144V25.48Z`}
      fill='#4A4C4F'
    />
  </SvgIcon>
);
