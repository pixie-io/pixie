import { Status } from 'types/generated/vizierapi_pb';

export type VizierQueryErrorType = 'script' | 'vis' | 'execution' | 'server';

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
      return 'Unexpected error';
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
}
