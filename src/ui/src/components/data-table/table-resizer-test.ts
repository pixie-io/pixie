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

import {
  MAX_COL_PX_WIDTH,
  MIN_COL_PX_WIDTH,
  userResizeColumn,
} from './table-resizer';

describe('Table resizing logic', () => {
  const CONTAINER_WIDTH = MAX_COL_PX_WIDTH * 1.25;

  function sum(widths: number[]) {
    return widths.reduce((total, item) => total + item, 0);
  }

  function widthsToSizes(widths: number[]) {
    const total = sum(widths);
    return widths.map((v, i) => ({
      key: String(i),
      ratio: v / total,
      isDefault: true,
    }));
  }

  function overflowingMinWidthColumns() {
    const numColumns = Math.ceil(CONTAINER_WIDTH / MIN_COL_PX_WIDTH) + 1; // 13
    const initialSizes = widthsToSizes(Array(numColumns).fill(MIN_COL_PX_WIDTH)); // 90x13 = 1170
    return { initialSizes, total: MIN_COL_PX_WIDTH * numColumns };
  }

  function underflowingMinWidthColumns() {
    const numColumns = Math.ceil((CONTAINER_WIDTH * 0.5) / MIN_COL_PX_WIDTH); // 5
    const initialSizes = widthsToSizes(Array(numColumns).fill(MIN_COL_PX_WIDTH)); // 90x5 = 450
    return { initialSizes, total: MIN_COL_PX_WIDTH * numColumns };
  }

  function underflowingMaxWidthColumns() {
    const numColumns = Math.ceil((CONTAINER_WIDTH * 0.5) / MAX_COL_PX_WIDTH); // 1 - tests too few columns as well
    const initialSizes = widthsToSizes(Array(numColumns).fill(MAX_COL_PX_WIDTH)); // 800x1 = 800
    return { initialSizes, total: MAX_COL_PX_WIDTH * numColumns };
  }

  function overflowingMaxWidthColumns() {
    const numColumns = Math.ceil((CONTAINER_WIDTH * 4) / MAX_COL_PX_WIDTH); // 5
    const initialSizes = widthsToSizes(Array(numColumns).fill(MAX_COL_PX_WIDTH)); // 800x5 = 4000
    return { initialSizes, total: MAX_COL_PX_WIDTH * numColumns };
  }

  function fittedFlexibleColumns() {
    const colWidth = CONTAINER_WIDTH / 4; // 250, more than min (90) but less than max (800)
    const initialSizes = widthsToSizes(Array(4).fill(colWidth)); // 1000
    return { initialSizes, total: CONTAINER_WIDTH, colWidth };
  }

  function overflowingFlexibleColumns() {
    const flexWidth = (MIN_COL_PX_WIDTH + MAX_COL_PX_WIDTH) / 2; // 485
    const numColumns = Math.ceil((CONTAINER_WIDTH * 2) / flexWidth); // 5
    const initialSizes = widthsToSizes(Array(numColumns).fill(flexWidth)); // 485 x 5 = 2425
    return { initialSizes, total: flexWidth * numColumns };
  }

  it('shrinks the total width before far columns, if table is wider than minimum', () => {
    const { initialSizes, total } = overflowingFlexibleColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '1',
      -10,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBeCloseTo(total - 10);
    expect(overrides[0]).toBeCloseTo(initialSizes[0].ratio);
    expect(overrides[1]).toBeCloseTo((overrides['0'] * total - 10) / newTotal);
    expect(overrides[2]).toBeCloseTo(initialSizes[2].ratio);
  });

  it('trades width between the two nearest columns before considering others', () => {
    const { initialSizes, total, colWidth } = fittedFlexibleColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '1',
      10,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    const newWidths = Object.keys(overrides).map((key) => overrides[key]);
    expect(newTotal).toBeCloseTo(total);
    expect(newWidths).toEqual([
      colWidth / newTotal,
      (colWidth + 10) / newTotal,
      (colWidth - 10) / newTotal,
      colWidth / newTotal,
    ]);
  });

  it('shrinks far columns proportionate to their limits, after table and closest column', () => {
    const initialSizes = widthsToSizes([
      MIN_COL_PX_WIDTH + 10,
      MIN_COL_PX_WIDTH + 20,
      MIN_COL_PX_WIDTH,
      MIN_COL_PX_WIDTH,
      MIN_COL_PX_WIDTH + 20,
      MIN_COL_PX_WIDTH + 10,
    ]);
    const total = MIN_COL_PX_WIDTH * 6 + 60;
    const { newTotal: leftTotal, sizes: leftOverrides } = userResizeColumn(
      '2',
      -3,
      initialSizes,
      total,
      total,
    );
    const { newTotal: rightTotal, sizes: rightOverrides } = userResizeColumn(
      '2',
      3,
      initialSizes,
      total,
      total,
    );
    const leftWidths = Object.keys(leftOverrides).map(
      (key) => leftOverrides[key] * leftTotal,
    );
    const rightWidths = Object.keys(rightOverrides).map(
      (key) => rightOverrides[key] * rightTotal,
    );
    const expectedLeft = [
      MIN_COL_PX_WIDTH + 9,
      MIN_COL_PX_WIDTH + 18,
      MIN_COL_PX_WIDTH,
      MIN_COL_PX_WIDTH + 3,
      MIN_COL_PX_WIDTH + 20,
      MIN_COL_PX_WIDTH + 10,
    ];
    const expectedRight = [
      MIN_COL_PX_WIDTH + 10,
      MIN_COL_PX_WIDTH + 20,
      MIN_COL_PX_WIDTH + 3,
      MIN_COL_PX_WIDTH,
      MIN_COL_PX_WIDTH + 18,
      MIN_COL_PX_WIDTH + 9,
    ];
    expect(leftTotal).toBeCloseTo(total);
    expect(rightTotal).toBeCloseTo(total);
    for (let i = 0; i < 6; i++) {
      expect(leftWidths[i]).toBeCloseTo(expectedLeft[i]);
      expect(rightWidths[i]).toBeCloseTo(expectedRight[i]);
    }
  });

  it('grows the table if width cannot be taken from other columns when dragging right', () => {
    const { initialSizes, total } = overflowingMinWidthColumns();
    const nextToLast = String(initialSizes.length - 2);
    const delta = 10; // As every column is already at its minimum width, this will force the table to grow to make room
    const { newTotal, sizes: overrides } = userResizeColumn(
      nextToLast,
      delta,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBeCloseTo(total + 10);
    expect(overrides[nextToLast]).toBeCloseTo(
      (MIN_COL_PX_WIDTH + 10) / newTotal,
    );
    for (const key of Object.keys(overrides)) {
      if (key !== nextToLast) {
        expect(overrides[key]).toBeCloseTo(initialSizes[key].ratio);
      }
    }
  });

  it('does not shrink any column below its minimum width', () => {
    const { initialSizes, total } = overflowingMinWidthColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '0',
      -1,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBeCloseTo(total);
    for (const key of Object.keys(overrides)) {
      expect(overrides[key]).toBeCloseTo(initialSizes[Number(key)].ratio);
    }
  });

  it('grows columns if needed to meet the minimum total width', () => {
    const { initialSizes, total } = underflowingMinWidthColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '0',
      0,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBeCloseTo(CONTAINER_WIDTH);
    const numColumns = Object.keys(overrides).length;
    for (const key of Object.keys(overrides)) {
      expect(overrides[key]).toBeCloseTo(1 / numColumns);
    }
  });

  it('when growing columns to meet the minimum total, increases individual maximums', () => {
    const { initialSizes, total } = underflowingMaxWidthColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '0',
      0,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBeCloseTo(CONTAINER_WIDTH);
    const numColumns = Object.keys(overrides).length;
    for (const key of Object.keys(overrides)) {
      expect(overrides[key]).toBeCloseTo(1 / numColumns);
    }
  });

  it('does not let any column nor the table grow past their maximums', () => {
    const { initialSizes, total } = overflowingMaxWidthColumns();
    const { newTotal, sizes: overrides } = userResizeColumn(
      '1',
      10,
      initialSizes,
      total,
      CONTAINER_WIDTH,
    );
    expect(newTotal).toBe(total);
    for (const key of Object.keys(overrides)) {
      expect(overrides[key]).toBeCloseTo(initialSizes[Number(key)].ratio);
    }
  });
});
