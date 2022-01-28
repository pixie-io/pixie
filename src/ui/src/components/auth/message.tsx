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

import { Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { CodeRenderer } from 'app/components/code-renderer/code-renderer';

import { PixienautBox, PixienautBoxProps } from './pixienaut-box';

const useStyles = makeStyles(({ palette, spacing }: Theme) => createStyles({
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
}), { name: 'AuthMessageBox' });

export interface AuthMessageBoxProps {
  error?: 'recoverable' | 'fatal';
  title: string;
  message: string;
  errorDetails?: string;
  code?: string;
  cta?: React.ReactNode;
}

export const AuthMessageBox = React.memo<AuthMessageBoxProps>(({
  error,
  errorDetails,
  title,
  message,
  code,
  cta,
}) => {
  const classes = useStyles();
  // eslint-disable-next-line react-memo/require-usememo
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
AuthMessageBox.displayName = 'AuthMessageBox';
