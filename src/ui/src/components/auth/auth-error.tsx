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

// eslint-disable-next-line react-memo/require-memo,react/display-name
export const AuthErrorSvg: React.FC<React.SVGProps<SVGSVGElement>> = (props) => (
  <svg {...props} width={77} height={69} viewBox='0 0 77 69' fill='none'>
    <path
      d={`M33.9651 7.68709C35.9355 4.27187 40.8645 4.27188 42.8349 7.68711L72.3701 58.8814C74.3393
        62.2948 71.8758 66.56 67.9352 66.56H8.86482C4.92417 66.56 2.46072 62.2948 4.42995
        58.8814L33.9651 7.68709Z`}
      fill='#FF5E6C'
    />
    <path
      d={`M41.8598 49.26H36.3998L35.5398 29.36H42.7198L41.8598 49.26ZM35.3398 55.46C35.3398
        54.4733 35.6931 53.6666 36.3998 53.04C37.1198 52.4 38.0131 52.08 39.0798 52.08C40.1465
        52.08 41.0331 52.4 41.7398 53.04C42.4598 53.6666 42.8198 54.4733 42.8198 55.46C42.8198
        56.4466 42.4598 57.26 41.7398 57.9C41.0331 58.5266 40.1465 58.84 39.0798 58.84C38.0131
        58.84 37.1198 58.5266 36.3998 57.9C35.6931 57.26 35.3398 56.4466 35.3398 55.46Z`}
      fill='#253238'
    />
  </svg>
);
