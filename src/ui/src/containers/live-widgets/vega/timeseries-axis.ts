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

import { format } from 'date-fns';
import * as vega from 'vega';
import { scale, tickValues } from 'vega-scale';
import { textMetrics } from 'vega-scenegraph';

declare module 'vega-scale' {
  // eslint-disable-next-line @typescript-eslint/no-shadow
  export function scale(type: string, scale?: any, metadata?: any);
  // eslint-disable-next-line @typescript-eslint/no-shadow
  export function tickValues(scale: any, count: number);
}

interface TextConfig {
  font: string;
  fontSize: number;
}

interface Label {
  tick: Date;
  label: string;
  formatter?: LabelsFormatter;
  pos: number;
}

interface LabelsFormatter {
  find: (labels: Label[]) => Label[];
  format: (label: Label) => string;
}

// Calculates overlap of left (l) and right (r) components in 1D land.
function overlap(l: Label, r: Label, overlapBuffer: number, textConfig: TextConfig,
  align = 'center'): boolean {
  if (align !== 'center') {
    throw TypeError('only "center" align supported');
  }
  const lWidth = textMetrics.width(textConfig, l.label);
  const rWidth = textMetrics.width(textConfig, r.label);
  return l.pos + lWidth / 2 + overlapBuffer > r.pos - rWidth / 2;
}

function hasOverlap(textConfig: TextConfig, labels: Label[], overlapBuffer: number): boolean {
  let b: Label;
  for (let i = 1, n = labels.length, a = labels[0]; i < n; a = b, i += 1) {
    b = labels[i];
    if (overlap(a, b, overlapBuffer, textConfig)) {
      return true;
    }
  }
  return false;
}

function applyFormat(formatter: LabelsFormatter, labels: Label[]) {
  /* eslint-disable no-param-reassign */
  formatter.find(labels).forEach((l) => {
    l.label = formatter.format(l);
    l.formatter = formatter;
  });
  /* eslint-enable no-param-reassign */
}

// Filters our every other label for overlap testing.
// This follows the default method used in
// `vega-view-transforms/src/Overlap.js`, but adds extra
// functionality to make sure special formatting of labels
// is preserved.
function parityReduce(labels: Label[]): Label[] {
  const specialFormatters = [];
  /* eslint-disable no-param-reassign */
  const newLabels = labels.filter((l, i) => {
    // Keep every other entry.
    if (i % 2 === 0) {
      return true;
    }
    l.label = '';
    // If this label-to-be-removed was formatted, we want re-run the formatter to make sure
    // that the formatting options still apply.
    if (l.formatter) {
      specialFormatters.push(l.formatter);
    }
    return false;
  });
  /* eslint-enable no-param-reassign */
  // Apply any special formatters that were found.
  specialFormatters.forEach((formatterFn) => { applyFormat(formatterFn, newLabels); });
  return newLabels;
}

// Formats the tick value into a time string given the options passed in.
export function formatTime(tick: Date, showAmPm = false, showDate = false): string {
  let fmtStr = 'h:mm:ss';
  if (showAmPm) {
    fmtStr += ' a';
    if (showDate) {
      fmtStr = `MMM dd, yyyy ${fmtStr}`;
    }
  }
  return format(tick, fmtStr);
}

function amPmFormatter(): LabelsFormatter {
  // Add AM and PM to the first and last labels.
  return {
    format: (label: Label): string => formatTime(label.tick, true),
    find: (labels: Label[]): Label[] => [labels[0], labels[labels.length - 1]],
  };
}

export function prepareLabels(
  // eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
  domain: any, width: number, numTicks: number, overlapBuffer: number, font: string, fontSize: number,
): Label[] {
  // Gets the true tick values that will be generated.
  const s = scale('time')();
  s.domain(domain);
  const ticks = tickValues(s, numTicks);

  // The config used to measure words.
  const textConfig: TextConfig = {
    font,
    fontSize,
  };

  // Label ticks will be evenly distributed. When alignment is centered
  const labelSep = width / ticks.length;
  const labelStart = 0;

  const labels = [];
  ticks.forEach((tick, i) => {
    labels.push({
      label: formatTime(tick),
      pos: labelStart + labelSep * i,
      tick,
    });
  });

  // Add AM/PM to the first and last labels.
  applyFormat(amPmFormatter(), labels);

  // We loop through the labels and "hide" (set to empty string) every other
  // label until we no longer have overlapping labels or we only have 3 elements showing.
  let items = labels;
  if (items.length >= 3 && hasOverlap(textConfig, items, overlapBuffer)) {
    do {
      items = parityReduce(items);
    } while (items.length >= 3 && hasOverlap(textConfig, items, overlapBuffer));
  }

  return labels;
}

// This adds the pxTimeFormat function to the passed in vega Module.
export function addPxTimeFormatExpression(): void {
  const domainFn = vega.expressionFunction('domain');

  // let currentWidth = 0;
  let labels = [];

  // Function call by labelExpr in the vega-lite config.
  function pxTimeFormat(datum, width, numTicks, separation, fontName, fontSize) {
    if (datum.index === 0) {
      // Generate the labels on the first run of this function.
      // Subsequent calls of pxTimeFormat will use these results.
      labels = prepareLabels(domainFn('x', this), width, numTicks, separation, fontName, fontSize);
      // currentWidth = width;
      // } else if (currentWidth !== width) {
      // TODO(philkuz) how should we warn when this happens?
      // console.warn('widths different', 'width', width, 'currentWidth', currentWidth);
    }

    // The denominator of the index.
    const indexDenom = labels.length - 1;

    // TrueIndex is the original index value.
    const trueIndex = Math.abs(Math.round(datum.index * indexDenom));
    // Backup if the trueIndex falls outside of the labels range.
    if (trueIndex >= labels.length) {
      // TODO(philkuz) how should we warn when this happens?
      // console.warn('trueIndex out of range, returning default format for datum. trueIndex:',
      //   datum.index * indexDenom);
      return formatTime(datum.value);
    }
    // Return the label we pre-calculated.
    return labels[trueIndex].label;
  }
  vega.expressionFunction('pxTimeFormat', pxTimeFormat);
}
