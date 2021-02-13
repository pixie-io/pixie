import { CompilerError, ErrorDetails, Status } from 'types/generated/vizierapi_pb';

import { ParseCompilerErrors } from './parse-compiler-errors';

jest.mock('containers/constants', () => ({ DOMAIN_NAME: 'dev.withpixie.dev' }));

function createCompilerDetailError({ line, col, msg }): ErrorDetails {
  const compilerErr = new CompilerError();
  compilerErr.setLine(line);
  compilerErr.setColumn(col);
  compilerErr.setMessage(msg);
  const detailErr = new ErrorDetails();
  detailErr.setCompilerError(compilerErr);
  return detailErr;
}

describe('ParseCompilerErrors test', () => {
  it('should return empty if no errors found.', () => {
    const mock = new Status();
    expect(ParseCompilerErrors(mock)).toEqual([]);
  });

  it('should return a single error when only a message is specified', () => {
    const mock = new Status();
    mock.setMessage('error error');
    expect(ParseCompilerErrors(mock)).toEqual([{ line: 0, column: 0, message: 'error error' }]);
  });

  it('should return multiple errors when specified', () => {
    const mock = new Status();
    const lineColErrors = [
      { line: 1, col: 1, msg: 'blah' },
      { line: 2, col: 2, msg: 'blahblah' },
    ].map(createCompilerDetailError);
    mock.setErrorDetailsList(lineColErrors);

    expect(ParseCompilerErrors(mock)).toEqual([
      { line: 1, column: 1, message: 'blah' },
      { line: 2, column: 2, message: 'blahblah' },
    ]);
  });

  it('should return multiple errors sorted properly', () => {
    const mock = new Status();
    const lineColErrors = [
      { line: 3, col: 1, msg: '3blah' },
      { line: 1, col: 3, msg: 'blah' },
      { line: 1, col: 1, msg: 'halb' },
      { line: 2, col: 1, msg: 'kdkd' },
    ].map(createCompilerDetailError);

    mock.setErrorDetailsList(lineColErrors);

    expect(ParseCompilerErrors(mock)).toEqual([
      { line: 1, column: 1, message: 'halb' },
      { line: 1, column: 3, message: 'blah' },
      { line: 2, column: 1, message: 'kdkd' },
      { line: 3, column: 1, message: '3blah' },
    ]);
  });
});
