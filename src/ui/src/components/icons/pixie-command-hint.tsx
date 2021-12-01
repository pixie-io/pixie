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

/* eslint-disable max-len */
import * as React from 'react';

import { SvgIcon, SvgIconProps } from '@mui/material';

// eslint-disable-next-line react-memo/require-memo,react/display-name
export const PixieCommandHint: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 924 283' fill='none' xmlns='http://www.w3.org/2000/svg'>
    <path
      d='M819 82H777.5V136C777.5 143.5 776 146.5 768 146.5H741V121.5L689 165L741 207V182H773C803.5 182 819 167 819 139V82Z'
      strokeWidth='10'
    />
    <rect x='589' y='5' width='330' height='273' rx='10' strokeWidth='10' />
    <rect x='5' y='4.99951' width='330' height='273' rx='10' strokeWidth='10' />
    <path
      d='M143.634 144.585V201H196.366V144.585H222L196.366 113.733L170 81.9995L143.634 113.733L118 144.585H143.634Z'
      strokeWidth='10'
    />
    <line x1='462' y1='101' x2='462' y2='183' strokeWidth='10' />
    <line x1='503' y1='142' x2='421' y2='142' strokeWidth='10' />
  </SvgIcon>
);
