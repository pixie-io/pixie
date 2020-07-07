import { SemanticType } from 'types/generated/vizier_pb';
import { parseQuantile, parseRows } from './parsers';

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

describe('parseRows', () => {
  it('correctly parses a row with quantiles', () => {
    const semanticTypes = new Map([
      ['quantileCol', SemanticType.ST_QUANTILES],
      ['nonQuantileCol', SemanticType.ST_NONE],
    ]);

    expect(parseRows(semanticTypes, [
      { quantileCol: '{"p10": 123, "p50": 345, "p90": 456, "p99": 789}', nonQuantileCol: '6' },
      { quantileCol: '{"p10": 123, "p50": 789, "p90": 1010, "p99": 2000}', nonQuantileCol: '5' },
    ]))
      .toStrictEqual([
        { quantileCol: { p50: 345, p90: 456, p99: 789 }, nonQuantileCol: '6' },
        { quantileCol: { p50: 789, p90: 1010, p99: 2000 }, nonQuantileCol: '5' },
      ]);
  });

  it('correctly returns unchanged rows with no types in need of special parsing', () => {
    const semanticTypes = new Map([
      ['quantileCol', SemanticType.ST_NONE],
      ['nonQuantileCol', SemanticType.ST_NONE],
    ]);

    const rows = [
      { quantileCol: '{"p10": 123, "p50": 345, "p90": 456, "p99": 789}', nonQuantileCol: '6' },
      { quantileCol: '{"p10": 123, "p50": 789, "p90": 1010, "p99": 2000}', nonQuantileCol: '5' },
    ];
    expect(parseRows(semanticTypes, rows)).toBe(rows);
  });
});
