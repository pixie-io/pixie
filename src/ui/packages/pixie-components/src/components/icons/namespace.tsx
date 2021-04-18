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

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const NamespaceIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 26 25'>
    <path
      d='M25 1H1V24H25V1Z'
      fill='none'
      stroke='currentColor'
      strokeWidth='1.04071'
      strokeMiterlimit='10'
      strokeLinejoin='round'
      strokeDasharray='2.08 1.04'
    />
    <path
      d={`M9.12613 9.7168L9.15543 10.3809C9.55907 9.87305 10.0864 9.61914
          10.7375 9.61914C11.854 9.61914 12.4171 10.249 12.4269
          11.5088V15H11.5236V11.5039C11.5203 11.123 11.4324 10.8415 11.2599
          10.6592C11.0907 10.4769 10.8254 10.3857 10.464 10.3857C10.1711
          10.3857 9.91389 10.4639 9.69254 10.6201C9.47118 10.7764 9.29866
          10.9814 9.17496 11.2354V15H8.27164V9.7168H9.12613ZM17.0155
          13.5986C17.0155 13.3545 16.9227 13.1657 16.7372 13.0322C16.5549
          12.8955 16.2342 12.7783 15.7752 12.6807C15.3195 12.583 14.9566
          12.4658 14.6864 12.3291C14.4194 12.1924 14.2209 12.0296 14.0907
          11.8408C13.9637 11.652 13.9002 11.4274 13.9002 11.167C13.9002
          10.734 14.0825 10.3678 14.4471 10.0684C14.8149 9.76888 15.2837
          9.61914 15.8534 9.61914C16.4523 9.61914 16.9373 9.77376 17.3084
          10.083C17.6828 10.3923 17.87 10.7878 17.87 11.2695H16.9618C16.9618
          11.0221 16.856 10.8089 16.6444 10.6299C16.436 10.4508 16.1724
          10.3613 15.8534 10.3613C15.5246 10.3613 15.2674 10.4329 15.0819
          10.5762C14.8963 10.7194 14.8036 10.9066 14.8036 11.1377C14.8036
          11.3558 14.8898 11.5202 15.0623 11.6309C15.2349 11.7415 15.5457
          11.8473 15.995 11.9482C16.4474 12.0492 16.8136 12.1696 17.0936
          12.3096C17.3735 12.4495 17.5802 12.6188 17.7137 12.8174C17.8504
          13.0127 17.9188 13.252 17.9188 13.5352C17.9188 14.0072 17.73
          14.3864 17.3524 14.6729C16.9748 14.9561 16.4849 15.0977 15.8827
          15.0977C15.4595 15.0977 15.0851 15.0228 14.7596 14.873C14.4341
          14.7233 14.1786 14.515 13.993 14.248C13.8107 13.9779 13.7196
          13.6865 13.7196 13.374H14.6229C14.6392 13.6768 14.7596 13.9176
          14.9842 14.0967C15.2121 14.2725 15.5116 14.3604 15.8827
          14.3604C16.2245 14.3604 16.4979 14.292 16.703 14.1553C16.9113
          14.0153 17.0155 13.8298 17.0155 13.5986Z`}
    />
  </SvgIcon>
);
