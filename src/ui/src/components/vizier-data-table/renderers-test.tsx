import { getMaxQuantile } from './renderers';

describe('getMaxQuantile', () => {
  it('correctly gets the max quantile for a set of rows (positive)', () => {
    const rows = [
      { quantiles: { p50: 10, p90: 60, p99: 89 } },
      { quantiles: { p50: 20, p90: 260, p99: 261 } },
      { quantiles: { p50: 30, p90: 360, p99: 370 } },
    ];
    expect(getMaxQuantile(rows, 'quantiles')).toBe(370);
  });

  it('correctly gets the max quantile for a set of rows (negative)', () => {
    const rows = [
      { quantiles: { p50: -10, p90: -6, p99: -3 } },
      { quantiles: { p50: -20, p90: -26, p99: -23 } },
      { quantiles: { p50: -30, p90: -36, p99: -33 } },
      { quantiles: null },
    ];
    expect(getMaxQuantile(rows, 'quantiles')).toBe(-3);
  });
});
