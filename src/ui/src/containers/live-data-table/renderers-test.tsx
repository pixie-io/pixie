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

import { getMaxQuantile } from './renderers';

describe('getMaxQuantile', () => {
  it('correctly gets the max quantile for a set of rows (positive)', () => {
    const rows = [
      { quantiles: { p50: 10, p90: 60, p99: 89 } },
      { quantiles: { p50: 20, p90: 260, p99: 261 } },
      { quantiles: { p50: 30, p90: 360, p99: 370 } },
    ];
    expect(getMaxQuantile(rows, 'quantiles')).toBe(370);
  });

  it('correctly gets the max quantile for a set of rows (negative)', () => {
    const rows = [
      { quantiles: { p50: -10, p90: -6, p99: -3 } },
      { quantiles: { p50: -20, p90: -26, p99: -23 } },
      { quantiles: { p50: -30, p90: -36, p99: -33 } },
      { quantiles: null },
    ];
    expect(getMaxQuantile(rows, 'quantiles')).toBe(-3);
  });
});
