import { Status } from 'types/generated/vizierapi_pb';

interface CompilerError {
  line: number;
  column: number;
  message: string;
}

function compare(err1: CompilerError, err2: CompilerError) {
  const lineDiff = err1.line - err2.line;
  if (lineDiff !== 0) {
    return lineDiff;
  }
  return err1.column - err2.column;
}

export function ParseCompilerErrors(status: Status): CompilerError[] {
  const out = [];
  const msg = status.getMessage();
  if (msg) {
    out.push({
      line: 0,
      column: 0,
      message: msg,
    });
  }
  const errors = status.getErrorDetailsList();
  for (const error of errors) {
    if (!error.hasCompilerError()) {
      continue;
    }
    const cErr = error.getCompilerError();
    out.push({
      line: cErr.getLine(),
      column: cErr.getColumn(),
      message: cErr.getMessage(),
    });
  }
  out.sort(compare);
  return out;
}
