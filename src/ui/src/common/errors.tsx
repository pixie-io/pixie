import * as React from 'react';
import { Status } from 'types/generated/vizier_pb';

import Link from '@material-ui/core/Link';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

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

const useStyles = makeStyles((theme: Theme) => createStyles({
  errorRow: {
    ...theme.typography.body2,
    fontFamily: '"Roboto Mono", Monospace',
    marginLeft: `-${theme.spacing(3.3)}px`,
    paddingBottom: theme.spacing(0.5),
  },
  link: {
    cursor: 'pointer',
  },
}));

export const VizierErrorDetails = (props: { error: Error }) => {
  const { error } = props;
  const classes = useStyles();
  const { details } = error as VizierQueryError;

  let errorDetails = null;

  if (typeof details === 'string') {
    errorDetails = <div className={classes.errorRow}>{details}</div>;
  } else if (Array.isArray(details)) {
    errorDetails = (
      <>
        {
          details.map((err, i) => <div key={i} className={classes.errorRow}>{err}</div>)
        }
      </>
    );
  } else {
    errorDetails = <div className={classes.errorRow}>{error.message}</div>;
  }
  return (
    <>
      {errorDetails}
      <div className={classes.errorRow}>
        Need help?&nbsp;
        <Link className={classes.link} id='intercom-trigger'>Chat with us</Link>
        .
      </div>
    </>
  );
};
