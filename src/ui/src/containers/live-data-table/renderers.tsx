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
import {
  AlertData,
  BytesRenderer,
  CPUData,
  DataWithUnits,
  DurationRenderer,
  formatDuration,
  HTTPStatusCodeRenderer,
  JSONData,
  PercentRenderer,
  PortRenderer, ThroughputBytesRenderer, ThroughputRenderer,
} from 'app/containers/format-data/format-data';
import { EmbedState } from 'app/containers/live-widgets/utils/live-view-params';
import {
  EntityLink,
  isEntityType,
  ScriptReference,
  STATUS_TYPES,
  toStatusIndicator,
} from 'app/containers/live-widgets/utils';
import { QuantilesBoxWhisker, SelectedPercentile } from 'app/components';
import { DataType, SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';
import {
  getDataRenderer,
  looksLikeAlertCol,
  looksLikeCPUCol,
  looksLikeLatencyCol, looksLikePIDCol,
} from 'app/utils/format-data';
import { getLatencyNSLevel, getColor } from 'app/utils/metric-thresholds';
import { Theme } from '@material-ui/core/styles';
import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

interface Quant { p50: number; p90: number; p99: number; }

// Expects a p99 field in colName.
export function getMaxQuantile(rows: any[], colName: string): number {
  let max: number = null;
  for (let i = 0; i < rows.length; ++i) {
    const row = rows[i];
    if (row[colName]) {
      const { p99 } = row[colName];
      if (p99 != null) {
        if (max != null) {
          max = Math.max(max ?? Number.NEGATIVE_INFINITY, p99);
        } else {
          max = p99;
        }
      }
    }
  }
  return max;
}

// Expects data to contain floats in p50, p90, and p99 fields.
export function quantilesRenderer(
  display: ColumnDisplayInfo,
  theme: Theme,
  updateDisplay: (ColumnDisplayInfo) => void,
  rows: any[],
): (v: Quant) => React.ReactElement {
  const max = getMaxQuantile(rows, display.columnName);

  return function renderer(val) {
    const { p50, p90, p99 } = val;
    const quantilesDisplay = display.displayState as QuantilesDisplayState;
    const selectedPercentile = quantilesDisplay.selectedPercentile || 'p99';

    const floatRenderer = getDataRenderer(DataType.FLOAT64);
    const p50Display = floatRenderer(p50);
    const p90Display = floatRenderer(p90);
    const p99Display = floatRenderer(p99);

    return (
      <QuantilesBoxWhisker
        p50={p50}
        p90={p90}
        p99={p99}
        max={max}
        p50Display={p50Display}
        p90Display={p90Display}
        p99Display={p99Display}
        p50HoverFill={getColor('none', theme)}
        p90HoverFill={getColor('none', theme)}
        p99HoverFill={getColor('none', theme)}
        selectedPercentile={selectedPercentile}
        onChangePercentile={(newPercentile: SelectedPercentile) => {
          updateDisplay({
            ...display,
            displayState: { selectedPercentile: newPercentile },
          });
        }}
      />
    );
  };
}

// Helper to durationQuantilesRenderer since it takes in a string, rather than a span
// for p50Display et al.
function dataWithUnitsToString(dataWithUnits: DataWithUnits): string {
  return `${dataWithUnits.val} ${dataWithUnits.units}`;
}

// Expects data to contain p50, p90, and p99 fields.
export function durationQuantilesRenderer(
  display: ColumnDisplayInfo,
  theme: Theme,
  updateDisplay: (ColumnDisplayInfo) => void,
  rows: any[],
): (val: Quant) => React.ReactElement {
  const max = getMaxQuantile(rows, display.columnName);

  return function renderer(val) {
    const { p50, p90, p99 } = val;
    const quantilesDisplay = display.displayState as QuantilesDisplayState;
    const selectedPercentile = quantilesDisplay.selectedPercentile || 'p99';
    let p50HoverFill = getColor('none', theme);
    let p90HoverFill = getColor('none', theme);
    let p99HoverFill = getColor('none', theme);

    // individual keys in ST_DURATION_NS_QUANTILES are FLOAT64 ST_DURATION_NS.
    if (looksLikeLatencyCol(display.columnName, SemanticType.ST_DURATION_NS, DataType.FLOAT64)) {
      p50HoverFill = getColor(getLatencyNSLevel(p50), theme);
      p90HoverFill = getColor(getLatencyNSLevel(p90), theme);
      p99HoverFill = getColor(getLatencyNSLevel(p99), theme);
    }

    const p50Display = dataWithUnitsToString(formatDuration(p50));
    const p90Display = dataWithUnitsToString(formatDuration(p90));
    const p99Display = dataWithUnitsToString(formatDuration(p99));

    return (
      <QuantilesBoxWhisker
        p50={p50}
        p90={p90}
        p99={p99}
        max={max}
        p50Display={p50Display}
        p90Display={p90Display}
        p99Display={p99Display}
        p50HoverFill={p50HoverFill}
        p90HoverFill={p90HoverFill}
        p99HoverFill={p99HoverFill}
        selectedPercentile={selectedPercentile}
        onChangePercentile={(newPercentile: SelectedPercentile) => {
          updateDisplay({
            ...display,
            displayState: { selectedPercentile: newPercentile },
          });
        }}
      />
    );
  };
}

function renderWrapper(RendererFunc: any /* TODO(zasgar): revisit this typing */) {
  return function renderer(val) {
    return <RendererFunc data={val} />;
  };
}

const statusRenderer = (st: SemanticType) => (v: string) => toStatusIndicator(v, st);

const serviceRendererFuncGen = (clusterName: string, embedState: EmbedState,
  propagatedArgs?: Arguments) => function Service(v) {
  try {
    // Hack to handle cases like "['pl/service1', 'pl/service2']" which show up for pods that are part of 2 services.
    const parsedArray = JSON.parse(v);
    if (Array.isArray(parsedArray)) {
      return (
        <>
          {
              parsedArray.map((entity, i) => (
                <span key={i}>
                  {i > 0 && ', '}
                  <EntityLink
                    entity={entity}
                    semanticType={SemanticType.ST_SERVICE_NAME}
                    clusterName={clusterName}
                    embedState={embedState}
                    propagatedParams={propagatedArgs}
                  />
                </span>
              ))
            }
        </>
      );
    }
  } catch (e) {
    // noop.
  }
  return <EntityLink
    entity={v}
    semanticType={SemanticType.ST_SERVICE_NAME}
    clusterName={clusterName}
    embedState={embedState}
    propagatedParams={propagatedArgs}
  />;
};

const entityRenderer = (st: SemanticType, clusterName: string, embedState: EmbedState,
  propagatedArgs?: Arguments) => {
  if (st === SemanticType.ST_SERVICE_NAME) {
    return serviceRendererFuncGen(clusterName, embedState, propagatedArgs);
  }
  return function Entity(v) {
    return (
      <EntityLink
        entity={v}
        semanticType={st}
        clusterName={clusterName}
        embedState={embedState}
        propagatedParams={propagatedArgs}
      />
    );
  };
};

const scriptReferenceRenderer = (clusterName: string, embedState: EmbedState,
  propagatedArgs?: Arguments) => (function Reference(v) {
  const { script, label, args } = v;
  const mergedArgs = { ...propagatedArgs, ...args };
  return (
    <ScriptReference
      label={label}
      clusterName={clusterName}
      script={script}
      embedState={embedState}
      args={mergedArgs}
    />
  );
});

const CPUDataWrapper = (data: any) => <CPUData data={data} />;
const AlertDataWrapper = (data: any) => <AlertData data={data} />;
const PlainNumberWrapper = (data: any) => <>{data}</>;

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export function prettyCellRenderer(
  display: ColumnDisplayInfo,
  updateDisplay: (ColumnDisplayInfo) => void,
  clusterName: string,
  rows: any[],
  theme: Theme,
  embedState: EmbedState,
  propagatedArgs?: Arguments,
) {
  const dt = display.type;
  const st = display.semanticType;
  const name = display.columnName;
  const renderer = getDataRenderer(dt);

  if (isEntityType(st)) {
    return entityRenderer(st, clusterName, embedState, propagatedArgs);
  }

  switch (st) {
    case SemanticType.ST_QUANTILES:
      return quantilesRenderer(display, theme, updateDisplay, rows);
    case SemanticType.ST_DURATION_NS_QUANTILES:
      return durationQuantilesRenderer(display, theme, updateDisplay, rows);
    case SemanticType.ST_PORT:
      return renderWrapper(PortRenderer);
    case SemanticType.ST_DURATION_NS:
      return renderWrapper(DurationRenderer);
    case SemanticType.ST_BYTES:
      return renderWrapper(BytesRenderer);
    case SemanticType.ST_HTTP_RESP_STATUS:
      return renderWrapper(HTTPStatusCodeRenderer);
    case SemanticType.ST_PERCENT:
      return renderWrapper(PercentRenderer);
    case SemanticType.ST_THROUGHPUT_PER_NS:
      return renderWrapper(ThroughputRenderer);
    case SemanticType.ST_THROUGHPUT_BYTES_PER_NS:
      return renderWrapper(ThroughputBytesRenderer);
    case SemanticType.ST_SCRIPT_REFERENCE:
      return scriptReferenceRenderer(clusterName, embedState, propagatedArgs);
    default:
      break;
  }

  if (STATUS_TYPES.has(st)) {
    return statusRenderer(st);
  }

  if (looksLikeCPUCol(name, st, dt)) {
    return CPUDataWrapper;
  }

  if (looksLikeAlertCol(name, dt)) {
    return AlertDataWrapper;
  }

  if (looksLikePIDCol(name, dt)) {
    return PlainNumberWrapper;
  }

  if (dt !== DataType.STRING) {
    return renderer;
  }

  return (v) => {
    try {
      const jsonObj = JSON.parse(v);
      return (
        <JSONData
          data={jsonObj}
        />
      );
    } catch {
      return v;
    }
  };
}

// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
export function liveCellRenderer(
  display: ColumnDisplayInfo,
  updateDisplay: (ColumnDisplayInfo) => void,
  prettyRender: boolean,
  theme: Theme,
  clusterName: string,
  rows: any[],
  embedState: EmbedState,
  propagatedArgs?: Arguments,
) {
  if (prettyRender) {
    return prettyCellRenderer(display, updateDisplay, clusterName, rows, theme, embedState, propagatedArgs);
  }
  return getDataRenderer(display.type);
}
