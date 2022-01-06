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

export type VizierQueryErrorType = 'script' | 'vis' | 'execution' | 'server' | 'unavailable';

export enum GRPCStatusCode {
  OK = 0,
  Cancelled,
  Unknown,
  InvalidArgument,
  DeadlineExceeded,
  NotFound,
  AlreadyExists,
  PermissionDenied,
  ResourceExhausted,
  FailedPrecondition,
  Aborted,
  OutOfRange,
  Unimplemented,
  Internal,
  Unavailable,
  DataLoss,
}

function getUserFacingMessage(errType: VizierQueryErrorType): string {
  switch (errType) {
    case 'vis':
      return 'Invalid Vis spec';
    case 'script':
      return 'Invalid PXL script';
    case 'execution':
      return 'Failed to execute script';
    case 'server':
      return 'Server error';
    case 'unavailable':
      return 'Transient error';
    default:
      // Not reached
      return 'Unknown error';
  }
}

export class VizierQueryError extends Error {
  constructor(
    public errType: VizierQueryErrorType,
    public details?: string | string[],
    public status?: Status,
  ) {
    super(getUserFacingMessage(errType));
  }

  toString(): string {
    const details = Array.isArray(this.details) ? this.details : [this.details];
    const detailsString = details.map((m) => `\n  ${m}`).join('');

    return `VizierQueryError(type=${this.errType}, status=${this.status}): ${this.message}${detailsString}`;
  }
}
