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
