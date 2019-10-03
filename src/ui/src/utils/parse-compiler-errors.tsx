// TODO(zasgar/michelle): Figure out how to import schema properly
import {
  GQLCompilerErrors,
} from '../../../vizier/services/api/controller/schema/schema';

export function ParseCompilerErrors(error: GQLCompilerErrors): any {
  if (!error) {
    // No errors available.
    return [];
  }

  // The data is stored in columnar format, this converts it to rows.
  const outputData = [];
  // Three columns: 1. line#, 2. col#, 3. msg
  if (error.msg && error.msg.trim() !== '') {
    const row = {
      line: 0,
      col: 0,
      msg: error.msg,
    };
    outputData.push(row);
  }

  if (!(error.lineColErrors && error.lineColErrors.length > 0)) {
    return outputData;
  }

  // function used to sort first by lines then by cols.
  function compare(err1, err2) {
    const lineDiff = err1.line - err2.line;
    if (lineDiff !== 0) {
      return lineDiff;
    }
    return err1.col - err2.col;
  }

  error.lineColErrors.sort(compare);

  return outputData.concat(error.lineColErrors);
}
