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
  clamp, rescaleSum, smallest, sumColumn,
} from 'app/utils/math';

export type StartingRatios = Array<{
  key: string;
  ratio: number;
  isDefault: boolean;
}>;

// Prevent any one column from dominating the viewport or becoming too thin to show its contents
export const MIN_COL_PX_WIDTH = 90;
export const MAX_COL_PX_WIDTH = 1800;

export interface ColWidthOverrides {
  [dataKey: string]: number;
}

/**
 * The actual resizing algorithm. Tries to transfer width from the side the drag handle is moving towards to the
 * side it's moving away from. The two columns closest to the drag handle are considered first, and then the remaining
 * amount to transfer is distributed proportionately among the farther columns of each side. This honors min/max widths.
 * If the table has room to grow or shrink, that will be used first (on the take side) or last (on the give side).
 *
 * @param targetDelta How much width to transfer from one side to the other. Treated the same regardless of sign.
 * @param currentWidths The current width of each column before transfer
 * @param deltaLimits Pre-computed limits per column to shrink (negative) or grow (positive) before reaching a limit.
 * @param tableDeltaLimit Pre-computed limit for the table itself (sum of all columns) to shrink or grow.
 * @param takeCloseIndex On the side the drag handle moves towards, which column is closest to the handle.
 * @param takeFarRange On the side the drag handle moves towards, all column indices except the closest to the handle.
 * @param giveCloseIndex On the side the drag handle moves away from, which column is closest to the handle.
 *        There is no giveFarRange; those columns are left alone.
 * @returns A redistributed list of widths for each column. Retains the same total width and number of columns.
 */
function giveTake(
  targetDelta: number,
  currentWidths: number[],
  deltaLimits: Array<[number, number]>,
  tableDeltaLimit: [number, number],
  takeCloseIndex: number,
  takeFarRange: [number, number],
  giveCloseIndex: number,
): number[] {
  // Made absolute to keep math consistent (which side is give and which is take is decided in the inputs)
  const absDelta = Math.abs(targetDelta);
  // In case ranges are out of bounds (due to there being too few columns on a side), consider only what actually exists
  const takeCloseLim = deltaLimits[takeCloseIndex] ?? [0, 0];
  const takeFarLims = deltaLimits
    .slice(...takeFarRange)
    .filter((lim) => lim !== undefined);
  const giveCloseLim = deltaLimits[giveCloseIndex] ?? [0, 0];

  // `Take` values/limits are all negative numbers, `give` values/limits are all positive numbers
  const maxTake = clamp(
    sumColumn(0, [...takeFarLims, takeCloseLim]),
    -1 * absDelta,
    0,
  );
  const maxGive = clamp(
    sumColumn(1, [giveCloseLim, tableDeltaLimit]),
    0,
    smallest(absDelta, maxTake),
  );
  let remainingTake = clamp(maxTake, -1 * maxGive, 0);

  const newWidths = [...currentWidths];
  const immediateTake = smallest(takeCloseLim[0], remainingTake);
  newWidths[takeCloseIndex] -= Math.abs(immediateTake);
  remainingTake -= immediateTake;
  const sumAvailableTake = sumColumn(0, takeFarLims);

  // If the table is wider than its container, taken width is spent on shrinking the whole before growing other columns
  const takeFromTable = clamp(tableDeltaLimit[0], immediateTake, 0);

  const takeRatio = sumAvailableTake ? remainingTake / sumAvailableTake : 0;
  for (
    let neighborIdx = takeFarRange[0];
    neighborIdx < takeFarRange[1];
    neighborIdx++
  ) {
    const takeAmount = Math.abs(takeRatio * deltaLimits[neighborIdx][0]);
    newWidths[neighborIdx] -= takeAmount;
  }

  // Same priorities when giving what was taken from the opposite side
  const immediateGive = smallest(
    giveCloseLim[1],
    absDelta,
    maxGive - Math.abs(takeFromTable),
  );
  newWidths[giveCloseIndex] += immediateGive;
  giveCloseLim[0] += immediateGive;
  giveCloseLim[1] -= immediateGive;
  if (
    targetDelta > 0
    && absDelta - immediateGive > 0
    && tableDeltaLimit[1] - immediateGive > 0
  ) {
    // There is more delta requested, and the table has room to grow. If the giveClose target still has room to grow,
    // give it as much as possible that doesn't exceed the table's growth limit.
    const giveToTable = smallest(
      absDelta - immediateGive,
      giveCloseLim[1],
      tableDeltaLimit[1] - immediateGive,
    );
    newWidths[giveCloseIndex] += giveToTable;
  }

  // May have a different sum than we started with, if the table shrank or grew as part of the operation
  return newWidths;
}

function ratiosToOverrides(ratios: StartingRatios): ColWidthOverrides {
  return ratios.reduce(
    (out, current) => ({
      ...out,
      [current.key]: current.isDefault ? undefined : current.ratio,
    }),
    {},
  );
}

function widthsToOverrides(
  total: number,
  oldRatios: StartingRatios,
  widths: number[],
) {
  return oldRatios.reduce(
    (result, old, i) => ({
      ...result,
      [old.key]: widths[i] / total,
    }),
    {},
  );
}

function enforceMinimumTotal(
  total: number,
  maxEach: number,
  startingValues: number[],
): number[] {
  const finalValues = [...startingValues];
  let currentTotal = startingValues.reduce((t, v) => t + v, 0);
  for (
    let growCandidate = startingValues.length - 1;
    growCandidate >= 0;
    growCandidate--
  ) {
    const diff = total - currentTotal;
    if (diff <= 0) break;
    const maxGrowth = Math.max(0, maxEach - startingValues[growCandidate]);
    const actual = Math.min(maxGrowth, diff);
    finalValues[growCandidate] += actual;
    currentTotal += actual;
  }
  return finalValues;
}

/**
 * Determines the allowed range for a table's total width, based on its columns and container
 */
export function tableWidthLimits(
  numColumns: number,
  containerWidth: number,
): [number, number] {
  const minTotal = Math.max(MIN_COL_PX_WIDTH * numColumns, containerWidth, 0);
  const maxTotal = Math.max(MAX_COL_PX_WIDTH * numColumns, containerWidth, 0);
  return [minTotal, maxTotal];
}

/**
 * Tries to move the drag handle between two columns, identified by the column left of that border. This resizes columns
 * on either side and the table's overall width as needed, but won't let either go beyond its min/max width in so doing.
 *
 * Checks where it can take width from (in the direction the drag handle is moving), where it can put that width (on the
 * opposite side of the handle), then does so in a prioritized series of steps.
 *
 * Priority when moving left: take from the closest left column, then other left columns. Give to the table as a whole
 * (by moving its rightmost border to the left), then the closest right column.
 * Priority when moving right: take from the closest right column, then the other right columns. Give to the closest
 * left column, then the table as a whole (by moving its rightmost border right). Leave farther left columns alone.
 *
 * @param dataKey Identifier of the column whose right border is being dragged
 * @param deltaX Difference, in pixels, between the border's previous location and where it wants to be now
 * @param ratios An in-order list of tuples with a column's name, and its width as a ratio of the whole table's width
 * @param startingTotal The current total width of all columns, in pixels. Used for min/max size constraints.
 * @param containerWidth The thinnest the table is allowed to be (usually the width of its container), in pixels. If
 * this is less than the sum of minimum column widths, that will be used instead.
 * @return A record mapping column names to their new ratios
 */
export function userResizeColumn(
  dataKey: string,
  deltaX: number,
  ratios: StartingRatios,
  startingTotal: number,
  containerWidth: number,
): { newTotal: number; sizes: ColWidthOverrides } {
  // TODO(nick) Known issue: the first resize of any column on a table with fewer columns than needed to fill the space
  //  causes all columns to snap into a more equally distributed arrangement (this only happens once). Not sure why yet.
  const colIdx = ratios.findIndex((col) => col.key === dataKey);
  if (colIdx === -1) {
    return { newTotal: startingTotal, sizes: ratiosToOverrides(ratios) };
  }

  // Enforce that the table is at least as wide as its container, and adjust column width limits accordingly.
  const [minTotal, maxTotal] = tableWidthLimits(ratios.length, containerWidth);
  const clampedTotal = clamp(startingTotal, minTotal, maxTotal);
  const contextualMaxColWidth = Math.max(maxTotal / ratios.length, MAX_COL_PX_WIDTH);

  // Transform the ratios into pixel widths for simpler logic and math. They get converted back at the end.
  const columnWidths = rescaleSum(ratios.map((r) => r.ratio), clampedTotal);
  // How much is the table allowed to shrink or grow before hitting an overall limit? [negative, positive]
  const tableDeltaLimit: [number, number] = [minTotal - clampedTotal, maxTotal - clampedTotal];
  // Same deal for individual columns. These limits may individually sum to more than the total delta limit.
  // The maximum limit may be overridden if it needs to be higher to meet the table's minimum width.
  const columnDeltaLimits: Array<[number, number]> = columnWidths.map((w) => [
    Math.min(MIN_COL_PX_WIDTH - w, 0),
    Math.max(contextualMaxColWidth - w, 0),
  ]);

  // Fix any prior errors that might have previously pushed column sizes out of bounds
  let newWidths = enforceMinimumTotal(
    startingTotal,
    contextualMaxColWidth,
    columnWidths,
  );

  if (deltaX < 0) {
    const takeCloseIndex = colIdx;
    const takeFarRange: [number, number] = [0, colIdx];
    const giveCloseIndex = colIdx + 1;
    const leftwardTableDeltaLimit: [number, number] = [tableDeltaLimit[0], 0];

    newWidths = giveTake(
      deltaX,
      newWidths,
      columnDeltaLimits,
      leftwardTableDeltaLimit,
      takeCloseIndex,
      takeFarRange,
      giveCloseIndex,
    );
  } else if (deltaX > 0) {
    const takeCloseIndex = colIdx + 1;
    const takeFarRange: [number, number] = [colIdx + 2, newWidths.length];
    const giveCloseIndex = colIdx;
    const rightwardTableDeltaLimit: [number, number] = [0, tableDeltaLimit[1]];

    newWidths = giveTake(
      deltaX,
      newWidths,
      columnDeltaLimits,
      rightwardTableDeltaLimit,
      takeCloseIndex,
      takeFarRange,
      giveCloseIndex,
    );
  }

  const finalTotal = clamp(newWidths.reduce((t, w) => t + w, 0), minTotal, maxTotal);

  // At this point, if remainingDelta is still nonzero, there was no way to accommodate it within width constraints.
  // Return the closest we could get, with overrides rescaled as ratios out of 1.
  return {
    newTotal: finalTotal,
    sizes: widthsToOverrides(finalTotal, ratios, newWidths),
  };
}
