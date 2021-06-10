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

import * as React from 'react';

import Link from '@material-ui/core/Link';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { VizierQueryError } from 'app/api';
import { CONTACT_ENABLED } from 'app/containers/constants';

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

export const VizierErrorDetails: React.FC<{ error: Error }> = ({ error }) => {
  const classes = useStyles();
  const { details } = error as VizierQueryError;

  let errorDetails: JSX.Element;

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
      {CONTACT_ENABLED && (
        <div className={classes.errorRow}>
          Need help?&nbsp;
          <Link className={classes.link} id='intercom-trigger'>Chat with us</Link>
          .
        </div>
      )}
    </>
  );
};
