import * as React from 'react';
import {
  AlertData, CPUData, JSONData, LatencyData,
} from 'components/format-data/format-data';
import {
  EntityLink,
  isEntityType, STATUS_TYPES, toStatusIndicator,
} from 'components/live-widgets/utils';
import QuantilesBoxWhisker, {
  SelectedPercentile,
} from 'components/quantiles-box-whisker/quantiles-box-whisker';
import { DataType, Relation, SemanticType } from 'types/generated/vizier_pb';
import {
  getDataRenderer, looksLikeAlertCol, looksLikeCPUCol, looksLikeLatencyCol,
} from 'utils/format-data';
import {
  getCPULevel, getLatencyLevel, GaugeLevel,
} from 'utils/metric-thresholds';
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

// Expects data to contain p50, p90, and p99 fields.
export function quantilesRenderer(display: ColumnDisplayInfo,
  updateDisplay: (ColumnDisplayInfo) => void, rows: any[]) {
  const max = getMaxQuantile(rows, display.columnName);

  return function renderer(val) {
    const { p50, p90, p99 } = val;
    const quantilesDisplay = display.displayState as QuantilesDisplayState;
    const selectedPercentile = quantilesDisplay.selectedPercentile || 'p99';
    let p50Level: GaugeLevel = 'none';
    let p90Level: GaugeLevel = 'none';
    let p99Level: GaugeLevel = 'none';
    // Can't pass in DataType here, which is STRING, but we know quantiles are floats.
    if (looksLikeLatencyCol(display.columnName, DataType.FLOAT64)) {
      p50Level = getLatencyLevel(p50);
      p90Level = getLatencyLevel(p90);
      p99Level = getLatencyLevel(p99);
    }
    if (looksLikeCPUCol(display.columnName, DataType.FLOAT64)) {
      p50Level = getCPULevel(p50);
      p90Level = getCPULevel(p90);
      p99Level = getCPULevel(p99);
    }
    return (
      <QuantilesBoxWhisker
        p50={p50}
        p90={p90}
        p99={p99}
        max={max}
        p50Level={p50Level}
        p90Level={p90Level}
        p99Level={p99Level}
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

const statusRenderer = (st: SemanticType) => (v: string) => toStatusIndicator(v, st);

const serviceRendererFuncGen = (clusterName: string) => (v) => {
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
                  <EntityLink entity={entity} semanticType={SemanticType.ST_SERVICE_NAME} clusterName={clusterName} />
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

const entityRenderer = (st: SemanticType, clusterName: string) => {
  if (st === SemanticType.ST_SERVICE_NAME) {
    return serviceRendererFuncGen(clusterName);
  }
  const entity = (v) => (
    <EntityLink entity={v} semanticType={st} clusterName={clusterName} />
  );
  return entity;
};

const LatencyDataWrapper = (data: any) => <LatencyData data={data} />;
const CPUDataWrapper = (data: any) => <CPUData data={data} />;
const AlertDataWrapper = (data: any) => <AlertData data={data} />;

export const prettyCellRenderer = (display: ColumnDisplayInfo, updateDisplay: (ColumnDisplayInfo) => void,
  clusterName: string, rows: any[]) => {
  const dt = display.type;
  const st = display.semanticType;
  const name = display.columnName;
  const renderer = getDataRenderer(dt);

  if (isEntityType(st)) {
    return entityRenderer(st, clusterName);
  }

  if (st === SemanticType.ST_QUANTILES) {
    return quantilesRenderer(display, updateDisplay, rows);
  }

  if (STATUS_TYPES.has(st)) {
    return statusRenderer(st);
  }

  if (looksLikeLatencyCol(name, dt)) {
    return LatencyDataWrapper;
  }

  if (looksLikeCPUCol(name, dt)) {
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
  prettyRender: boolean, clusterName: string, rows: any[]) => {
  if (prettyRender) {
    return prettyCellRenderer(display, updateDisplay, clusterName, rows);
  }
  return getDataRenderer(display.type);
};
