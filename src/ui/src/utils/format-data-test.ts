import { DataType, UInt128 } from 'types/generated/vizier_pb';
import * as FormatData from './format-data';

describe('looksLikeLatencyCol test', () => {
  it('should not accept non-float latency columns', () => {
    expect(FormatData.looksLikeLatencyCol('latency', DataType.STRING)).toEqual(false);
  });

  it('should not accept incorrectly named columns', () => {
    expect(FormatData.looksLikeLatencyCol('CPU', DataType.FLOAT64)).toEqual(false);
  });

  it('should accept FLOAT64 columns with correct naming', () => {
    expect(FormatData.looksLikeLatencyCol('latency', DataType.FLOAT64)).toEqual(true);
  });
});

describe('looksLikeAlertCol test', () => {
  it('should not accept non-boolean alert columns', () => {
    expect(FormatData.looksLikeAlertCol('alert', DataType.STRING)).toEqual(false);
  });

  it('should not accept incorrectly named columns', () => {
    expect(FormatData.looksLikeAlertCol('CPU', DataType.BOOLEAN)).toEqual(false);
  });

  it('should accept BOOLEAN columns with correct naming', () => {
    expect(FormatData.looksLikeAlertCol('alert', DataType.BOOLEAN)).toEqual(true);
  });
});

describe('formatFloat64Data test', () => {
  it('should accept decimal-rendered scientific notation', () => {
    // 1e-6 renders to "0.000001" in numeral.js internally
    expect(FormatData.formatFloat64Data(1e-6)).toEqual('0');
  });
  it('should accept scientific notation rendered scientific notation', () => {
    // 1e-6 renders to '1e-7' internally in numeral.js, usually throws NaNs
    expect(FormatData.formatFloat64Data(1e-7)).toEqual('0');
  });
  it('should render NaNs correctly', () => {
    expect(FormatData.formatFloat64Data(NaN)).toEqual('NaN');
  });
  it('should render normal floats correctly', () => {
    expect(FormatData.formatFloat64Data(123.456)).toEqual('123.46');
  });
  it('should render huge floats correctly', () => {
    expect(FormatData.formatFloat64Data(123.456789101)).toEqual('123.46');
  });
});

describe('formatUInt128Protobuf test', () => {
  it('should format to an uuid string', () => {
    const val = new UInt128();
    val.setHigh(77311094061);
    val.setLow(34858981);
    expect(FormatData.formatUInt128Protobuf(val)).toEqual('00000012-0019-ad2d-0000-00000213e7e5');
  });
});
