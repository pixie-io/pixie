import { parseQuantile } from './parsers';

describe('parseQuantile', () => {
  it('correctly parses a quantile', () => {
    expect(parseQuantile('{"p10": 123, "p50": 345, "p90": 456, "p99": 789}'))
      .toStrictEqual({ p50: 345, p90: 456, p99: 789 });
  });

  it('returns null for a non-quantile', () => {
    expect(parseQuantile('px-sock-shop'))
      .toStrictEqual(null);
  });
});
