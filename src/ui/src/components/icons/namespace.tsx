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
export const NamespaceIcon: React.FC<SvgIconProps> = (props) => (
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
      d={`M8.39307 9.13184L8.4502 9.92529C8.94108 9.31169 9.59912 9.00488
      10.4243 9.00488C11.1522 9.00488 11.6938 9.21859 12.0493 9.646C12.4048
      10.0734 12.5868 10.7124 12.5952 11.563V16H10.7607V11.6074C10.7607
      11.2181 10.6761 10.9367 10.5068 10.7632C10.3376 10.5854 10.0562 10.4966
      9.6626 10.4966C9.14632 10.4966 8.75911 10.7166 8.50098
      11.1567V16H6.6665V9.13184H8.39307ZM17.7114 14.1021C17.7114 13.8778
      17.5993 13.7021 17.375 13.5752C17.1549 13.444 16.7995 13.3276 16.3086
      13.2261C14.6751 12.8833 13.8584 12.1893 13.8584 11.144C13.8584 10.5347
      14.1102 10.0269 14.6138 9.62061C15.1216 9.21012 15.7839 9.00488 16.6006
      9.00488C17.4723 9.00488 18.1685 9.21012 18.689 9.62061C19.2137 10.0311
      19.4761 10.5643 19.4761 11.2202H17.6416C17.6416 10.9578 17.557 10.742
      17.3877 10.5728C17.2184 10.3993 16.9539 10.3125 16.5942 10.3125C16.2853
      10.3125 16.0462 10.3823 15.877 10.522C15.7077 10.6616 15.623 10.8394
      15.623 11.0552C15.623 11.2583 15.7183 11.4233 15.9087 11.5503C16.1034
      11.673 16.4292 11.7809 16.8862 11.874C17.3433 11.9629 17.7284 12.0645
      18.0415 12.1787C19.0106 12.5342 19.4951 13.1499 19.4951 14.0259C19.4951
      14.6522 19.2264 15.16 18.689 15.5493C18.1515 15.9344 17.4575 16.127
      16.6069 16.127C16.0314 16.127 15.5194 16.0254 15.0708 15.8223C14.6265
      15.6149 14.2773 15.3335 14.0234 14.978C13.7695 14.6183 13.6426 14.2311
      13.6426 13.8164H15.3818C15.3988 14.1423 15.5194 14.3919 15.7437
      14.5654C15.9679 14.7389 16.2684 14.8257 16.645 14.8257C16.9963 14.8257
      17.2607 14.7601 17.4385 14.6289C17.6204 14.4935 17.7114 14.3179 17.7114 14.1021Z`}
    />
  </SvgIcon>
);
