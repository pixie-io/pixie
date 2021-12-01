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
export const ServiceIcon: React.FC<SvgIconProps> = (props) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      d={`M10.3056 6H21.6924V11.0448H10.3056V6ZM2.6001
        20.9552H9.78634V25.9999H2.6001V20.9552ZM12.4065
        20.9552H19.5927V25.9999H12.4065V20.9552ZM29.3991
        20.9552H22.2128V25.9999H29.3991V20.9552ZM15.3251
        15.3465V11.0482H15.3454V11.045H15.3457H16.6528H16.6529V15.3467H25.8006C26.1615
        15.3467 26.4541 15.6393 26.4541
        16.0003V20.9492H25.147V16.6538H16.6739V20.9525H15.3664V16.654H6.8516V20.9492H5.54407V16.0003C5.54407
        15.6392 5.83677 15.3465 6.19784 15.3465H15.3251Z`}
    />
  </SvgIcon>
);
