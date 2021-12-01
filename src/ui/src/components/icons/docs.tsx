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
export const DocsIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M6.8 5H24.4C25.3941 5 26.2 5.80589 26.2 6.8V26C26.2 26.9941 25.3941 27.8 24.4
        27.8H6.8C5.80589 27.8 5 26.9941 5 26V6.8C5 5.80589 5.80589 5 6.8 5ZM24.2 25.8V7H7V25.8H24.2Z
       `}
    />
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M16.5082 12.5797C16.2594 12.8258 15.96 12.9488 15.61 12.9488C15.26 12.9488 14.9619 12.8258
        14.7158 12.5797C14.4697 12.3336 14.3467 12.0355 14.3467 11.6855C14.3467 11.3355 14.4697 11.0361
        14.7158 10.7873C14.9619 10.5385 15.26 10.4141 15.61 10.4141C15.9654 10.4141 16.2662 10.5385
        16.5123 10.7873C16.7584 11.0361 16.8815 11.3355 16.8815 11.6855C16.8815 12.0355 16.757 12.3336
        16.5082 12.5797ZM17.6854 21.5047V21.8H13.5428V21.5047C13.8818 21.4938 14.1334 21.3953 14.2975
        21.2094C14.4068 21.0836 14.4615 20.75 14.4615 20.2086V15.7297C14.4615 15.1883 14.3986 14.8424
        14.2729 14.692C14.1471 14.5416 13.9037 14.4555 13.5428 14.4336V14.1301H16.7584V20.2086C16.7584
        20.75 16.8213 21.0959 16.9471 21.2463C17.0729 21.3967 17.319 21.4828 17.6854 21.5047Z
       `}
    />
  </SvgIcon>
);
