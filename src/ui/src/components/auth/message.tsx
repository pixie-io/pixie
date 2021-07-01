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
import {
  alpha,
  Theme,
  Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import { CodeRenderer } from 'app/components/code-renderer/code-renderer';
import { PixienautBox, PixienautBoxProps } from './pixienaut-box';

const styles = ({ palette, spacing }: Theme) => createStyles({
  root: {
    backgroundColor: alpha(palette.foreground.grey3, 0.8),
    paddingLeft: spacing(6),
    paddingRight: spacing(6),
    paddingTop: spacing(10),
    paddingBottom: spacing(10),
    boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
    borderRadius: spacing(3),
  },
  title: {
    color: palette.foreground.two,
  },
  message: {
    color: palette.foreground.one,
    marginTop: spacing(5.5),
    marginBottom: spacing(4),
  },
  errorDetails: {
    color: palette.foreground.grey4,
    marginTop: spacing(4),
  },
  extraPadding: {
    height: spacing(6),
  },
  code: {
    width: '100%',
  },
});

export interface AuthMessageBoxProps extends WithStyles<typeof styles> {
  error?: 'recoverable'|'fatal';
  title: string;
  message: string;
  errorDetails?: string;
  code?: string;
  cta?: React.ReactNode;
}

export const AuthMessageBox = withStyles(styles)((props: AuthMessageBoxProps) => {
  const {
    error,
    errorDetails,
    title,
    message,
    code,
    cta,
    classes,
  } = props;
  let scenario: PixienautBoxProps['image'] = 'balloon';
  if (error) {
    scenario = error === 'recoverable' ? 'octopus' : 'toilet';
  }
  return (
    <PixienautBox image={scenario}>
      <Typography variant='h1' className={classes.title}>
        {title}
      </Typography>
      <Typography variant='h6' className={classes.message}>
        {message}
      </Typography>
      {code && (
        <div className={classes.code}>
          <CodeRenderer
            code={code}
          />
        </div>
      )}
      {error && errorDetails && (
        <Typography variant='body1' className={classes.errorDetails}>
          {`Details: ${errorDetails}`}
        </Typography>
      )}
      {cta}
      <div className={classes.extraPadding} />
    </PixienautBox>
  );
});
