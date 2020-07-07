import { SemanticType } from 'types/generated/vizier_pb';

export interface Quantile {
  p50: number;
  p90: number;
  p99: number;
}

// Parses a JSON string as a quantile, so that downstream sort and renderers don't
// have to reparse the JSON every time they handle a quantiles value.
export function parseQuantile(val: any): Quantile {
  try {
    const parsed = JSON.parse(val);
    const { p50, p90, p99 } = parsed;
    return { p50, p90, p99 };
  } catch (error) {
    return null;
  }
}

// Parses the rows based on their semantic type.
export function parseRows(semanticTypeMap: Map<string, SemanticType>, rows: any[]): any[] {
  const parsers = new Map();
  semanticTypeMap.forEach((semanticType, dataKey) => {
    if (semanticType === SemanticType.ST_QUANTILES) {
      parsers.set(dataKey, parseQuantile);
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
