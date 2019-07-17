import * as ResultDataUtils from './result-data-utils';

describe('ResultsToCsv test', () => {
  it('should correctly format results', () => {
    const results = '{"relation":{"columns":[{"columnName":"time_","columnType":"TIME64NS"},' +
      '{"columnName":"http_request","columnType":"STRING"}]},' +
      '"rowBatches": [{"cols":[{"time64nsData":{"data":["1","2","3","4"]}},' +
      '{"stringData":{"data":["a","b","c","d"]}}],"numRows":"4"},' +
      '{"cols":[{"time64nsData":{"data":["5","6","7","8"]}},' +
      '{"stringData":{"data":["{\\"req_id\\": \\"123\\", \\"req_id2\\": \\"{456}\\"}","f","g","h"]}}],' +
      '"numRows":"4"}]}';

    expect(ResultDataUtils.ResultsToCsv(results)).toEqual(
      'time_,http_request\n"1","a"\n"2","b"\n"3","c"\n"4","d"\n"5",' +
      '"""{\\\\"\"req_id\\\\"\": \\\\"\"123\\\\"\", \\\\"\"req_id2\\\\"\": \\\\"\"{456}\\\\"\"}"""' +
      '\n"6","f"\n"7","g"\n"8","h"\n',
    );
  });
});
