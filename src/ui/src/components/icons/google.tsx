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
export const GoogleIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon
    {...props}
    width='16'
    height='16'
    viewBox='0 0 16 16'
    fill='none'
    xmlns='http://www.w3.org/2000/svg'
  >
    <path
      d='M9.88619 4.97263L11.8299 3.38131C10.7908 2.51858 9.45597
    2 8.00015 2C5.68126 2 3.66963 3.31537 2.67105 5.24062C2.24214
    6.06677 2 7.00502 2 7.99996C2 9.02887 2.2588 9.99709 2.71505 10.8434C3.72847
    12.7228 5.7151 14 8.00004 14C9.4219 14 10.7281 13.5055 11.7563 12.679C12.8135
    11.8291 13.5768 10.6281 13.8689 9.25271C13.9549 8.84853 14.0001 8.42951 14.0001
    7.99995C14.0001 7.61687 13.9641 7.24203 13.8952 6.87908H8.13196V9.25271H11.3403C11.0515
    10.0235 10.5033 10.6673 9.80175 11.0787C9.2733 11.389 8.6574 11.5667 8.00004
    11.5667C6.4707 11.5667 5.16619 10.604 4.6595 9.25174C4.51313 8.86242 4.4333
    8.4404 4.43326 7.99996C4.43326 7.59414 4.50118 7.20414 4.6258 6.84084C5.10711 5.44005
    6.43619 4.43328 8.00015 4.43328C8.69278 4.43328 9.33928 4.63077 9.88619 4.97263Z'
      fill='#121212'
    />
  </SvgIcon>
);
