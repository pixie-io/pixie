import {ParseCompilerErrors} from './parse-compiler-errors';

jest.mock('containers/constants', () => ({ DOMAIN_NAME: 'dev.withpixie.dev' }));

describe('ParseCompilerErrors test', () => {
  it('should return empty if no errors found.', () => {
    const mock = null;
    expect(ParseCompilerErrors(mock)).toEqual([]);
  });

  it('should return a single error when only a message is specified', () => {
    const mock = {
        msg: 'error error',
        lineColErrors: [],
    };
    expect(ParseCompilerErrors(mock)).toEqual([{line: 0, col: 0, msg: 'error error'}]);
  });

  it('should return multiple errors when specified', () => {
    const mock = {
        msg: '',
        lineColErrors: [{line: 1, col: 1, msg: 'blah'}, {line: 2, col: 2, msg: 'blahblah'}],
    };
    expect(ParseCompilerErrors(mock)).toEqual([{line: 1, col: 1, msg: 'blah'}, {line: 2, col: 2, msg: 'blahblah'}]);
  });
  it('should return multiple errors sorted properly', () => {
    const mock = {
        msg: '',
        lineColErrors: [
          {line: 3, col: 1, msg: '3blah'},
          {line: 1, col: 3, msg: 'blah'},
          {line: 1, col: 1, msg: 'halb'},
          {line: 2, col: 1, msg: 'kdkd'}],
    };
    expect(ParseCompilerErrors(mock)).toEqual([
          {line: 1, col: 1, msg: 'halb'},
          {line: 1, col: 3, msg: 'blah'},
          {line: 2, col: 1, msg: 'kdkd'},
          {line: 3, col: 1, msg: '3blah'},
    ]);
  });
});
