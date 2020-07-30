import { SemanticType } from 'types/generated/vizier_pb';

export interface Quantile {
  p50: number;
  p90: number;
  p99: number;
}

export interface ContainerStatus {
  state: string;
  reason: string;
  message: string;
}

export interface PodStatus {
  phase: string;
  reason: string;
  message: string;
}

// Parses a JSON string as a quantile, so that downstream sort and renderers don't
// have to reparse the JSON every time they handle a quantiles value.
function parseQuantile(val: any): Quantile {
  try {
    const parsed = JSON.parse(val);
    const { p50, p90, p99 } = parsed;
    return { p50, p90, p99 };
  } catch (error) {
    return null;
  }
}

function parseContainerStatus(val: any): ContainerStatus {
  try {
    const parsed = JSON.parse(val);
    const { state, reason, message } = parsed;
    return { state, reason, message };
  } catch (error) {
    return null;
  }
}

function parsePodStatus(val: any): PodStatus {
  try {
    const parsed = JSON.parse(val);
    const { phase, reason, message } = parsed;
    return { phase, reason, message };
  } catch (error) {
    return null;
  }
}

// Parses the rows based on their semantic type.
export function parseRows(semanticTypeMap: Map<string, SemanticType>, rows: any[]): any[] {
  const parsers = new Map();
  semanticTypeMap.forEach((semanticType, dataKey) => {
    switch (semanticType) {
      case SemanticType.ST_CONTAINER_STATUS:
        parsers.set(dataKey, parseContainerStatus);
        break;
      case SemanticType.ST_POD_STATUS:
        parsers.set(dataKey, parsePodStatus);
        break;
      case SemanticType.ST_QUANTILES:
        parsers.set(dataKey, parseQuantile);
        break;
      default:
        break;
    }
  });

  if (parsers.size === 0) {
    return rows;
  }

  return rows.map((row) => {
    const newValues = {};
    parsers.forEach((parser, dataKey) => {
      newValues[dataKey] = parser(row[dataKey]);
    });
    return {
      ...row,
      ...newValues,
    };
  });
}
