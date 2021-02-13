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
} from 'containers/format-data/format-data';
import {
  EntityLink,
  isEntityType,
  ScriptReference,
  STATUS_TYPES,
  toStatusIndicator,
} from 'containers/live-widgets/utils';
import { QuantilesBoxWhisker, SelectedPercentile } from 'pixie-components';
import { DataType, SemanticType } from 'types/generated/vizierapi_pb';
import { Arguments } from 'utils/args-utils';
import {
  getDataRenderer,
  looksLikeAlertCol,
  looksLikeCPUCol,
  looksLikeLatencyCol,
} from 'utils/format-data';
import { getLatencyNSLevel, getColor } from 'utils/metric-thresholds';
import { Theme } from '@material-ui/core/styles';
import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

// Expects a p99 field in colName.
export function getMaxQuantile(rows: any[], colName: string): number {
  let max;
  for (let i = 0; i < rows.length; ++i) {
    const row = rows[i];
    if (row[colName]) {
      const { p99 } = row[colName];
      if (p99 != null) {
        if (max != null) {
          max = Math.max(max, p99);
        } else {
          max = p99;
        }
      }
    }
  }
  return max;
}

// Expects data to contain floats in p50, p90, and p99 fields.
export function quantilesRenderer(display: ColumnDisplayInfo, theme: Theme,
  updateDisplay: (ColumnDisplayInfo) => void, rows: any[]) {
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
export function durationQuantilesRenderer(display: ColumnDisplayInfo, theme: Theme,
  updateDisplay: (ColumnDisplayInfo) => void, rows: any[]) {
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

const serviceRendererFuncGen = (clusterName: string, propagatedArgs?: Arguments) => (v) => {
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
  return <EntityLink entity={v} semanticType={SemanticType.ST_SERVICE_NAME} clusterName={clusterName} />;
};

const entityRenderer = (st: SemanticType, clusterName: string, propagatedArgs?: Arguments) => {
  if (st === SemanticType.ST_SERVICE_NAME) {
    return serviceRendererFuncGen(clusterName, propagatedArgs);
  }
  const entity = (v) => (
    <EntityLink
      entity={v}
      semanticType={st}
      clusterName={clusterName}
      propagatedParams={propagatedArgs}
    />
  );
  return entity;
};

const scriptReferenceRenderer = (clusterName: string) => ((v) => {
  const { script, label, args } = v;
  return (
    <ScriptReference label={label} script={script} args={args} clusterName={clusterName} />
  );
});

const CPUDataWrapper = (data: any) => <CPUData data={data} />;
const AlertDataWrapper = (data: any) => <AlertData data={data} />;

export const prettyCellRenderer = (display: ColumnDisplayInfo, updateDisplay: (ColumnDisplayInfo) => void,
  clusterName: string, rows: any[], theme: Theme, propagatedArgs?: Arguments) => {
  const dt = display.type;
  const st = display.semanticType;
  const name = display.columnName;
  const renderer = getDataRenderer(dt);

  if (isEntityType(st)) {
    return entityRenderer(st, clusterName, propagatedArgs);
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
      return scriptReferenceRenderer(clusterName);
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
};

export const vizierCellRenderer = (display: ColumnDisplayInfo, updateDisplay: (ColumnDisplayInfo) => void,
  prettyRender: boolean, theme: Theme, clusterName: string, rows: any[], propagatedArgs?: Arguments) => {
  if (prettyRender) {
    return prettyCellRenderer(display, updateDisplay, clusterName, rows, theme, propagatedArgs);
  }
  return getDataRenderer(display.type);
};
