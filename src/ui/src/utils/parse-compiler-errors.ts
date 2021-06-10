/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { Status } from 'app/types/generated/vizierapi_pb';

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
