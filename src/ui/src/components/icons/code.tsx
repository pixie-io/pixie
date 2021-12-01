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
export const CodeIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon
    {...props}
    width='14'
    height='14'
    viewBox='0 0 14 14'
    fill='none'
    xmlns='http://www.w3.org/2000/svg'
  >
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M14 7C14 10.866 10.866 14 7 14C3.13401 14 0 10.866 0 7C0 3.13401 3.13401 0 7 0C10.866 0 14 3.13401
        14 7ZM5.22284 6.11017C5.22284 6.34746 5.16205 6.54651 5.04047 6.70735C4.91888 6.86817 4.77531 6.96704
        4.60976 7.00396C4.77531 7.04087 4.91888 7.13842 5.04047 7.29661C5.16205 7.4548 5.22284 7.65254 5.22284
        7.88983V9.40847C5.22284 9.52976 5.25 9.61808 5.30432 9.67345C5.35865 9.72881 5.44013 9.7565 5.54878
        9.7565H6.08426V10.5H5.21508C4.915 10.5 4.68348 10.4156 4.52051 10.2469C4.35754 10.0782 4.27605 9.8435
        4.27605 9.54294V8.00847C4.27605 7.79755 4.23984 7.64463 4.16741 7.54972C4.09497 7.4548 3.96563 7.39944
        3.77938 7.38362L3.5 7.35989V6.64011L3.77938 6.61638C3.96563 6.60056 4.09497 6.5452 4.16741 6.45028C4.23984
        6.35537 4.27605 6.20245 4.27605 5.99153V4.45706C4.27605 4.1565 4.35754 3.92185 4.52051 3.75311C4.68348
        3.58437 4.915 3.5 5.21508 3.5H6.08426V4.2435H5.54878C5.44013 4.2435 5.35865 4.27119 5.30432 4.32655C5.25
        4.38192 5.22284 4.47024 5.22284 4.59153V6.11017ZM10.2206 6.61638L10.5 6.64011V7.35989L10.2206 7.38362C10.0344
        7.39944 9.90503 7.4548 9.83259 7.54972C9.76016 7.64463 9.72395 7.79755 9.72395 8.00847V9.54294C9.72395 9.8435
        9.64246 10.0782 9.47949 10.2469C9.31652 10.4156 9.085 10.5 8.78492 10.5H7.91574V9.7565H8.45122C8.55987 9.7565
        8.64135 9.72881 8.69568 9.67345C8.75 9.61808 8.77716 9.52976 8.77716 9.40847V7.88983C8.77716 7.65254 8.83795
        7.4548 8.95953 7.29661C9.08112 7.13842 9.22469 7.04087 9.39024 7.00396C9.22469 6.96704 9.08112 6.86817 8.95953
        6.70735C8.83795 6.54651 8.77716 6.34746 8.77716 6.11017V4.59153C8.77716 4.47024 8.75 4.38192 8.69568
        4.32655C8.64135 4.27119 8.55987 4.2435 8.45122 4.2435H7.91574V3.5H8.78492C9.085 3.5 9.31652 3.58437 9.47949
        3.75311C9.64246 3.92185 9.72395 4.1565 9.72395 4.45706V5.99153C9.72395 6.20245 9.76016 6.35537 9.83259
        6.45028C9.90503 6.5452 10.0344 6.60056 10.2206 6.61638Z`}
      fill='#B2B5BB'
    />
  </SvgIcon>
);
