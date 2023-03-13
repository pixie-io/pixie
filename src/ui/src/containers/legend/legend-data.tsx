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

import { View } from 'vega-typings';

import { COLOR_SCALE } from 'app/containers/live/convert-to-vega-spec/convert-to-vega-spec';

export interface LegendEntry {
  key: string;
  val: string;
  color: string;
}

export interface LegendData {
  time: string;
  entries: LegendEntry[];
}

const formatLegendEntry = (scale, key: string, val: number, formatter: (number) => string): LegendEntry => ({
  color: scale(key),
  key,
  val: formatter(val),
});

export const formatLegendData = (
  view: View,
  time: number,
  entries: UnformattedLegendEntry[],
  formatter: (val: number) => string,
): LegendData => {
  const legendData: LegendData = {
    time: new Date(time).toLocaleString(),
    entries: [],
  };
  // Handle the case where `COLOR_SCALE` doesn't exist in vega scales.
  let scale: any;
  try {
    scale = (view as any).scale(COLOR_SCALE);
  } catch (err) {
    // This shouldn't happen but if it does, return empty legend data.
    return legendData;
  }
  legendData.entries = entries.map((entry) => formatLegendEntry(
    scale,
    entry.key,
    entry.val,
    formatter,
  ));
  return legendData;
};

interface UnformattedLegendEntry {
  key: string;
  val: number;
}

// TimeHashMap is used to store the entries for the legend for any given time.
// This way we can lookup a time's legend entries in O(1).
interface TimeHashMap {
  [time: number]: UnformattedLegendEntry[];
}

// HoverDataCache is used to cache data about the hover legend. It stores both
// the TimeHashMap, as well as the min and max times of this chart.
// The min/max time are used to ensure the hover line never goes outside the graph.
// This cache is updated whenever vega's data changes.
export interface HoverDataCache {
  timeHashMap: TimeHashMap;
  minTime: number;
  maxTime: number;
}

interface ValidHoverDatum {
  time: number;
  [key: string]: number;
}

const minMaxTimes = (hoverData: ValidHoverDatum[]): { minTime: number; maxTime: number } => {
  const sortedTimes = hoverData.map((datum) => datum.time).sort();
  if (sortedTimes.length === 0) {
    return { minTime: 0, maxTime: Number.MAX_SAFE_INTEGER };
  }
  if (sortedTimes.length < 3) {
    return { minTime: sortedTimes[0], maxTime: sortedTimes[sortedTimes.length - 1] };
  }
  // Because of the time removal hack we have to use the secondmin and second max times, otherwise
  // the hover line will appear off of the graph.
  const secondMinTime = sortedTimes[1];
  let secondMaxTime: number;
  if (sortedTimes.length === 3) {
    // If there are only 3 items, both the secondmin and secondmax will be the middle item.
    secondMaxTime = sortedTimes[1];
  } else {
    secondMaxTime = sortedTimes[sortedTimes.length - 2];
  }
  return {
    minTime: secondMinTime,
    maxTime: secondMaxTime,
  };
};

const keyAvgs = (hoverData: ValidHoverDatum[]): { [key: string]: number } => {
  const keyedAvgState: { [key: string]: { sum: number; n: number } } = {};
  hoverData.forEach((datum) => {
    Object.entries(datum).forEach(([key, val]) => {
      if (key === 'time') { return; }
      if (!keyedAvgState[key]) {
        keyedAvgState[key] = { sum: 0.0, n: 0 };
      }
      keyedAvgState[key].sum += val;
      keyedAvgState[key].n += 1;
    });
  });
  return Object.fromEntries(
    Object.entries(keyedAvgState).map(([key, state]) => {
      if (state.n === 0) {
        return [key, 0.0];
      }
      return [key, state.sum / state.n];
    }),
  );
};

function buildTimeHashMap(
  hoverData: ValidHoverDatum[],
  sortBy: (a: UnformattedLegendEntry, b: UnformattedLegendEntry) => number,
): TimeHashMap {
  const timeHashMap: TimeHashMap = {};
  for (const datum of hoverData) {
    const rest: UnformattedLegendEntry[] = Object.entries(datum).map((entry) => ({ key: entry[0], val: entry[1] }))
      .filter((item) => item.key !== 'time' && item.key !== 'sum').sort(sortBy);
    timeHashMap[datum.time] = rest;
  }
  return timeHashMap;
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export const buildHoverDataCache = (hoverData: any): HoverDataCache => {
  if (!Array.isArray(hoverData)) {
    return null;
  }
  const validEntries = hoverData.map((entry) => {
    // eslint-disable-next-line @typescript-eslint/naming-convention
    const { time_, ...rest } = entry;
    if (!time_ || typeof time_ !== 'number') {
      return null;
    }
    const hoverDatum: ValidHoverDatum = {
      time: time_,
    };
    Object.entries(rest).forEach(([key, val]) => {
      if (typeof val !== 'number') {
        return;
      }
      hoverDatum[key] = val;
    });
    return hoverDatum;
  }).filter((datum) => datum);

  if (validEntries.length === 0) {
    return null;
  }

  const { minTime, maxTime } = minMaxTimes(validEntries);
  const keyedAvgs = keyAvgs(validEntries);

  const timeHashMap: TimeHashMap = buildTimeHashMap(validEntries, (a, b) => keyedAvgs[b.key] - keyedAvgs[a.key]);

  return {
    timeHashMap,
    minTime,
    maxTime,
  };
};
