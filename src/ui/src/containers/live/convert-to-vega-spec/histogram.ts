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

import { WidgetDisplay } from 'app/containers/live/vis';
import { Relation } from 'app/types/generated/vizierapi_pb';

import { barChartInternal } from './bar';
import {
  addDataSource,
  BASE_SPEC,
  DisplayWithLabels,
  extendDataTransforms,
  getVegaFormatFunc,
  TRANSFORMED_DATA_SOURCE_NAME,
  VegaSpecWithProps,
} from './common';

export interface Histogram {
  readonly value: string;
  readonly maxbins?: number;
  readonly minstep?: number;
  readonly horizontal?: boolean;
  readonly prebinCount: string;
}

export interface HistogramDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly histogram: Histogram;
}

export function convertToHistogramChart(
  display: HistogramDisplay,
  source: string,
  relation?: Relation,
): VegaSpecWithProps {
  if (!display.histogram) {
    throw new Error('HistogramChart must have an entry for property histogram');
  }
  if (!display.histogram.value) {
    throw new Error('HistogramChart property histogram must have an entry for property value');
  }
  // TODO(philkuz) support non-prebinned histograms.
  if (!display.histogram.prebinCount) {
    throw new Error('HistogramChart property histogram must have an entry for the prebinField');
  }

  const spec = { ...BASE_SPEC, style: 'cell' };

  // Add data and transforms.
  const baseDataSrc = addDataSource(spec, { name: source });
  const transformedDataSrc = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME, source: baseDataSrc.name, transform: [],
  });

  const binName = `bin_${display.histogram.value}`;
  const extentSignal = `${binName}_extent`;
  const binSignal = `${binName}_bins`;
  const binStart = binName;
  const binEnd = `${binName}_end`;
  const countField = `${display.histogram.value}_count`;

  const DEFAULT_MAX_BINS = 10;
  const DEFAULT_MIN_STEP = 0.0;
  // Groups which to aggregate by, will include groupby and stackby if we add those to histogram.
  const groups: string[] = [];
  // Setup histograms.
  extendDataTransforms(transformedDataSrc, [
    {
      type: 'extent',
      field: display.histogram.value,
      signal: extentSignal,
    },
    {
      type: 'bin',
      field: display.histogram.value,
      as: [binStart, binEnd],
      signal: binSignal,
      extent: {
        signal: extentSignal,
      },
      maxbins: display.histogram.maxbins || DEFAULT_MAX_BINS,
      minstep: display.histogram.minstep || DEFAULT_MIN_STEP,
    },
    {
      type: 'aggregate',
      groupby: [binStart, binEnd, ...groups],
      ops: ['sum'],
      fields: [display.histogram.prebinCount],
      as: [countField],
    },
  ]);

  const formatFuncMD = getVegaFormatFunc(relation, display.histogram.value);
  return barChartInternal({
    barStart: binStart,
    barEnd: binEnd,
    horizontal: display.histogram.horizontal,
    value: countField,
    barFmtFn: formatFuncMD,
    transformedDataSrc,
    labelScale: {
      domain: {
        signal: `[${binSignal}.start, ${binSignal}.stop]`,
      },
      type: 'linear',
      bins: { signal: binSignal },
    },
    display,
  }, spec);
}
