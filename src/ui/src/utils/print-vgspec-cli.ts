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

/* eslint-disable no-console */
import { DARK_THEME } from 'app/components';
import { Data } from 'vega';

import {
  BAR_CHART_TYPE,
  ChartDisplay,
  convertWidgetDisplayToVegaSpec,
  TIMESERIES_CHART_TYPE,
  HISTOGRAM_CHART_TYPE,
} from 'app/containers/live/convert-to-vega-spec';
import { DISPLAY_TYPE_KEY } from 'app/containers/live/vis';

const timeseriesData = [
  { time_: '4/2/2020, 9:42:38 PM', service: 'px-sock-shop/catalogue', bytesPerSecond: 48259 },
  { time_: '4/2/2020, 9:42:38 PM', service: 'px-sock-shop/orders', bytesPerSecond: 234 },
  { time_: '4/2/2020, 9:42:39 PM', service: 'px-sock-shop/catalogue', bytesPerSecond: 52234 },
  { time_: '4/2/2020, 9:42:39 PM', service: 'px-sock-shop/orders', bytesPerSecond: 23423 },
  { time_: '4/2/2020, 9:42:40 PM', service: 'px-sock-shop/catalogue', bytesPerSecond: 18259 },
  { time_: '4/2/2020, 9:42:40 PM', service: 'px-sock-shop/orders', bytesPerSecond: 28259 },
  { time_: '4/2/2020, 9:42:41 PM', service: 'px-sock-shop/catalogue', bytesPerSecond: 38259 },
  { time_: '4/2/2020, 9:42:42 PM', service: 'px-sock-shop/orders', bytesPerSecond: 10259 },
  { time_: '4/2/2020, 9:42:43 PM', service: 'px-sock-shop/catalogue', bytesPerSecond: 58259 },
];

const barData = [
  {
    service: 'carts', endpoint: '/create', cluster: 'prod', numErrors: 14,
  },
  {
    service: 'carts', endpoint: '/create', cluster: 'staging', numErrors: 60,
  },
  {
    service: 'carts', endpoint: '/create', cluster: 'dev', numErrors: 3,
  },
  {
    service: 'carts', endpoint: '/create', cluster: 'prod', numErrors: 80,
  },
  {
    service: 'carts', endpoint: '/create', cluster: 'staging', numErrors: 38,
  },
  {
    service: 'carts', endpoint: '/update', cluster: 'dev', numErrors: 55,
  },
  {
    service: 'carts', endpoint: '/submit', cluster: 'prod', numErrors: 11,
  },
  {
    service: 'carts', endpoint: '/submit', cluster: 'staging', numErrors: 58,
  },
  {
    service: 'carts', endpoint: '/submit', cluster: 'dev', numErrors: 79,
  },
  {
    service: 'orders', endpoint: '/remove', cluster: 'prod', numErrors: 83,
  },
  {
    service: 'orders', endpoint: '/remove', cluster: 'staging', numErrors: 87,
  },
  {
    service: 'orders', endpoint: '/remove', cluster: 'dev', numErrors: 67,
  },
  {
    service: 'orders', endpoint: '/add', cluster: 'prod', numErrors: 97,
  },
  {
    service: 'orders', endpoint: '/add', cluster: 'staging', numErrors: 84,
  },
  {
    service: 'orders', endpoint: '/add', cluster: 'dev', numErrors: 90,
  },
  {
    service: 'orders', endpoint: '/add', cluster: 'prod', numErrors: 74,
  },
  {
    service: 'orders', endpoint: '/new', cluster: 'staging', numErrors: 64,
  },
  {
    service: 'orders', endpoint: '/new', cluster: 'dev', numErrors: 19,
  },
  {
    service: 'frontend', endpoint: '/orders', cluster: 'prod', numErrors: 57,
  },
  {
    service: 'frontend', endpoint: '/orders', cluster: 'staging', numErrors: 35,
  },
  {
    service: 'frontend', endpoint: '/orders', cluster: 'dev', numErrors: 49,
  },
  {
    service: 'frontend', endpoint: '/redirect', cluster: 'prod', numErrors: 91,
  },
  {
    service: 'frontend', endpoint: '/signup', cluster: 'staging', numErrors: 38,
  },
  {
    service: 'frontend', endpoint: '/signup', cluster: 'dev', numErrors: 91,
  },
  {
    service: 'frontend', endpoint: '/signup', cluster: 'prod', numErrors: 99,
  },
  {
    service: 'frontend', endpoint: '/signup', cluster: 'staging', numErrors: 80,
  },
  {
    service: 'frontend', endpoint: '/signup', cluster: 'dev', numErrors: 37,
  },
];

const histogramData = [
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 150,
    count: 28,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 500,
    count: 3,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 100,
    count: 68,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 7050,
    count: 1,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1000,
    count: 99,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 2150,
    count: 1,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 3000,
    count: 16,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1050,
    count: 32,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 5050,
    count: 7,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 50,
    count: 168,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 0,
    count: 1402,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1100,
    count: 17,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 900,
    count: 1,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 5000,
    count: 14,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1950,
    count: 1,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 400,
    count: 4,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 800,
    count: 3,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 3100,
    count: 5,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 550,
    count: 4,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 3050,
    count: 13,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 950,
    count: 2,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 450,
    count: 2,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 250,
    count: 11,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 200,
    count: 8,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 7100,
    count: 6,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1150,
    count: 3,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 300,
    count: 7,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 700,
    count: 2,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 750,
    count: 3,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 350,
    count: 7,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 3150,
    count: 1,
  },
  {
    _tableName_: 'output',
    _clusterID_: '8eb3f3cc-eb20-44c8-b5ac-c17d07dc46e0',
    requestLatencyMS: 1300,
    count: 1,
  },
];

function printSpec(display: ChartDisplay) {
  const sourceName = 'mysource';
  const { spec, error } = convertWidgetDisplayToVegaSpec(display, sourceName, DARK_THEME);
  if (error) {
    console.log('Error compiling spec', error);
    return;
  }
  let data: Array<Record<string, any>>;

  if (display[DISPLAY_TYPE_KEY] === BAR_CHART_TYPE) {
    data = barData;
  } else if (display[DISPLAY_TYPE_KEY] === HISTOGRAM_CHART_TYPE) {
    data = histogramData;
  } else if (display[DISPLAY_TYPE_KEY] === TIMESERIES_CHART_TYPE) {
    data = timeseriesData;
  } else {
    console.log(
      `This tool only supports bar,timeseries, or histogram charts, not ${display[DISPLAY_TYPE_KEY]}`);
    return;
  }

  for (const datum of (spec.data as Data[])) {
    if (datum.name === sourceName) {
      (datum as any).values = data;
    }
  }

  // Remove everything that uses custom extensions to vega so that the spec works out of the box in
  // the online vega editor.
  if ((spec as any).axes) {
    for (const axis of (spec as any).axes) {
      if (axis.scale === 'x' && axis.encode && axis.encode.labels && axis.encode.labels.update
        && axis.encode.labels.update.text && axis.encode.labels.update.text.signal
        && axis.encode.labels.update.text.signal.includes('pxTimeFormat')) {
        axis.encode.labels.update.text.signal = '';
      }
    }
  }

  console.log(JSON.stringify(spec));
}

// Example input, replace with what you want to print the spec for.
const input = {
  '@type': HISTOGRAM_CHART_TYPE,
  histogram: {
    value: 'requestLatencyMS',
    prebinCount: 'count',
    maxbins: 20,
    horizontal: true,
  },
};

// const input = {
//   '@type': BAR_CHART_TYPE,
//   bar: {
//     value: 'numErrors',
//     label: 'service',
//     stackBy: 'endpoint',
//     horizontal: true,
//     groupBy: 'cluster',
//   },
// };

printSpec(input);

/* eslint-enable no-console */
