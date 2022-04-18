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

import { useTheme } from '@mui/material/styles';

import { VizierTable } from 'app/api';
import { QuantilesBoxWhisker, SelectedPercentile } from 'app/components';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import {
  AlertData,
  BytesRenderer,
  CPUData,
  dataWithUnitsToString,
  BasicDurationRenderer,
  formatDuration,
  HTTPStatusCodeRenderer,
  LatencyDurationRenderer,
  PercentRenderer,
  PortRenderer,
  ThroughputBytesRenderer,
  ThroughputRenderer,
} from 'app/containers/format-data/format-data';
import { JSONData } from 'app/containers/format-data/json-data';
import {
  DeepLink,
  ScriptReference,
  STATUS_TYPES,
  toStatusIndicator,
} from 'app/containers/live-widgets/utils';
import { semanticTypeDeepLinks } from 'app/containers/live-widgets/utils/live-view-params';
import { DataType, SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments, stableSerializeArgs } from 'app/utils/args-utils';
import {
  getDataRenderer,
  looksLikeAlertCol,
  looksLikeCPUCol,
  looksLikeLatencyCol,
  looksLikePIDCol,
} from 'app/utils/format-data';
import { getColor, getLatencyNSLevel } from 'app/utils/metric-thresholds';

import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

interface Quant { p50: number; p90: number; p99: number; }

interface LiveCellProps {
  data: any;
}

// Use this to render a cheap placeholder cell at first, then a full render in the next run of the event loop.
function useIsPlaceholder() {
  const [isPlaceholder, setIsPlaceholder] = React.useState(true);
  React.useEffect(() => {
    const delay = setTimeout(() => setIsPlaceholder(false), 0);
    return () => clearTimeout(delay);
  }, []);
  return isPlaceholder;
}

function getDeepLinkCellRenderer(
  st: SemanticType, clusterName: string, propagatedArgs?: Arguments,
): React.ComponentType<LiveCellProps> {
  const Renderer = React.memo<LiveCellProps>(({ data }) => {
    const { embedState } = React.useContext(LiveRouteContext);

    let values: string[] = [data];

    if (st === SemanticType.ST_SERVICE_NAME) {
      try {
        const parsed = JSON.parse(data);
        if (Array.isArray(parsed)) {
          values = parsed;
        }
      } catch { /**/ }
    }

    const components = values.map((value, i) => (
      <React.Fragment key={i}>
        {i > 0 && ', '}
        <DeepLink
          value={value}
          semanticType={st}
          clusterName={clusterName}
          embedState={embedState}
          propagatedParams={propagatedArgs}
        />
      </React.Fragment>
    ));

    return <>{components}</>;
  });
  Renderer.displayName = 'EntityCell';
  return Renderer;
}

function getQuantilesCellRenderer(
  display: ColumnDisplayInfo,
  updateDisplay: (ColumnDisplayInfo) => void,
  max = 0,
  isDuration = false,
): React.ComponentType<LiveCellProps> {
  const formatFloat = isDuration
    ? ((val: number) => dataWithUnitsToString(formatDuration(val)))
    : getDataRenderer(DataType.FLOAT64);

  const Renderer = React.memo<LiveCellProps>(({ data }) => {
    const theme = useTheme();
    const fill = React.useMemo(() => getColor('none', theme), [theme]);

    const onChangePercentile = React.useCallback((newPercentile: SelectedPercentile) => {
      updateDisplay({
        ...display,
        displayState: { selectedPercentile: newPercentile },
      });
    }, []);

    const isPlaceholder = useIsPlaceholder();
    if (isPlaceholder) return <>...</>;

    const { p50, p90, p99 } = data as Quant;

    /* eslint-disable react-memo/require-usememo */
    let p50HoverFill = fill;
    let p90HoverFill = fill;
    let p99HoverFill = fill;
    /* eslint-enable react-memo/require-usememo */

    // individual keys in ST_DURATION_NS_QUANTILES are FLOAT64 ST_DURATION_NS.
    if (isDuration && looksLikeLatencyCol(display.columnName, SemanticType.ST_DURATION_NS, DataType.FLOAT64)) {
      p50HoverFill = getColor(getLatencyNSLevel(p50), theme);
      p90HoverFill = getColor(getLatencyNSLevel(p90), theme);
      p99HoverFill = getColor(getLatencyNSLevel(p99), theme);
    }

    return (
      <QuantilesBoxWhisker
        p50={p50}
        p90={p90}
        p99={p99}
        max={max}
        p50Display={formatFloat(p50)}
        p90Display={formatFloat(p90)}
        p99Display={formatFloat(p99)}
        p50HoverFill={p50HoverFill}
        p90HoverFill={p90HoverFill}
        p99HoverFill={p99HoverFill}
        selectedPercentile={(display.displayState as QuantilesDisplayState).selectedPercentile || 'p99'}
        onChangePercentile={onChangePercentile}
      />
    );
  });
  Renderer.displayName = 'QuantilesCell';
  return Renderer;
}

function getScriptReferenceCellRenderer(
  clusterName: string, propagatedArgs?: Arguments,
): React.ComponentType<LiveCellProps> {
  const Renderer = React.memo<LiveCellProps>(({ data }) => {
    const { embedState } = React.useContext(LiveRouteContext);
    const { script, label, args } = data;
    const stabilizedArgs = stableSerializeArgs(args);
    // eslint-disable-next-line react-hooks/exhaustive-deps
    const mergedArgs = React.useMemo(() => ({ ...propagatedArgs, ...args }), [stabilizedArgs]);

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
  Renderer.displayName = 'ScriptReferenceCell';
  return Renderer;
}

const JSONCell = React.memo<LiveCellProps>(({ data }) => {
  const isPlaceholder = useIsPlaceholder();
  if (isPlaceholder) return <>{data}</>;

  try {
    const parsed = JSON.parse(data);
    return <JSONData data={parsed} />;
  } catch {
    return <>{data}</>;
  }
});
JSONCell.displayName = 'JSONCell';

const PlainCell = React.memo<LiveCellProps>(({ data }) => <>{data}</>);
PlainCell.displayName = 'PlainCell';

function getDataCell(formatter: (data: any) => string) {
  const Renderer = React.memo<LiveCellProps>(({ data }) => <>{formatter(data)}</>);
  Renderer.displayName = 'DataCell';
  return Renderer;
}

function getDurationRenderer(
  columnName: string, st: SemanticType, dt: DataType,
): React.ComponentType<LiveCellProps> {
  const isLatency = looksLikeLatencyCol(columnName, st, dt);
  if (isLatency) return LatencyDurationRenderer;
  return BasicDurationRenderer;
}

export function getLiveCellRenderer(
  table: VizierTable,
  display: ColumnDisplayInfo,
  updateDisplay: (ColumnDisplayInfo) => void,
  clusterName: string,
  propagatedArgs?: Arguments,
): React.ComponentType<LiveCellProps> {
  const { type: dt, semanticType: st, columnName } = display;
  const dataRenderer = getDataRenderer(dt);

  if (semanticTypeDeepLinks(st)) {
    return getDeepLinkCellRenderer(st, clusterName, propagatedArgs);
  }

  switch (st) {
    case SemanticType.ST_QUANTILES:
      return getQuantilesCellRenderer(display, updateDisplay, table.maxQuantiles.get(display.columnName), false);
    case SemanticType.ST_DURATION_NS_QUANTILES:
      return getQuantilesCellRenderer(display, updateDisplay, table.maxQuantiles.get(display.columnName), true);
    case SemanticType.ST_PORT:
      return PortRenderer;
    case SemanticType.ST_DURATION_NS:
      return getDurationRenderer(columnName, st, dt);
    case SemanticType.ST_BYTES:
      return BytesRenderer;
    case SemanticType.ST_HTTP_RESP_STATUS:
      return HTTPStatusCodeRenderer;
    case SemanticType.ST_PERCENT:
      return PercentRenderer;
    case SemanticType.ST_THROUGHPUT_PER_NS:
      return ThroughputRenderer;
    case SemanticType.ST_THROUGHPUT_BYTES_PER_NS:
      return ThroughputBytesRenderer;
    case SemanticType.ST_SCRIPT_REFERENCE:
      return getScriptReferenceCellRenderer(clusterName, propagatedArgs);
    default:
      break;
  }

  if (STATUS_TYPES.has(st)) return getDataCell((data) => toStatusIndicator(data, st));

  if (looksLikeCPUCol(columnName, st, dt)) return CPUData;
  if (looksLikeAlertCol(columnName, dt)) return AlertData;
  if (looksLikePIDCol(columnName, dt)) return PlainCell;

  if (dt !== DataType.STRING) return getDataCell(dataRenderer);

  return JSONCell;
}
