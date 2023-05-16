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

// This is the primary auth box, which has either the login or signin variant.
import * as React from 'react';

import { Button, Link, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import pixieAnalytics from 'app/utils/analytics';
import { WithChildren } from 'app/utils/react-boilerplate';

import { PixienautBox } from './pixienaut-box';

const useStyles = makeStyles(({ spacing, palette }: Theme) => createStyles({
  bodyText: {
    margin: 0,
  },
  account: {
    color: palette.foreground.grey4,
    textAlign: 'center',
  },
  title: {
    color: palette.foreground.two,
    paddingTop: spacing(1),
    paddingBottom: spacing(3),
    marginBottom: spacing(1.25),
  },
  subtitle: {
    color: palette.foreground.two,
    paddingTop: spacing(1.25),
    paddingBottom: spacing(5.25),
    marginBottom: spacing(1.25),
    textAlign: 'center',
  },
  gutter: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'center',
    alignItems: 'center',
    marginTop: spacing(3),
    paddingTop: spacing(1),
    paddingBottom: spacing(1),
    borderTop: `1px solid ${palette.foreground.grey1}`,
  },
  centerSelf: {
    alignSelf: 'center',
  },
  disclaimer: {
    fontStyle: 'italic',
    marginBottom: spacing(3),
  },
}), { name: 'AuthBox' });

export interface AuthBoxProps {
  title: string;
  body: string;
  buttonCaption: string;
  buttonText: string;
  toggleURL?: string;
  showTOSDisclaimer?: boolean;
}

export const AuthBox: React.FC<WithChildren<AuthBoxProps>> = React.memo((props) => {
  const {
    toggleURL,
    showTOSDisclaimer,
    title,
    body,
    buttonCaption,
    buttonText,
    children,
  } = props;
  const classes = useStyles();

  const reportToggleClick = React.useCallback(() => {
    pixieAnalytics.track('Switched between Login and Signup', { newUrl: toggleURL });
  }, [toggleURL]);

  return (
    <PixienautBox>
      <Typography variant='h1' className={classes.title}>
        {title}
      </Typography>
      <Typography variant='subtitle1' className={classes.subtitle}>
        <span>
          {body.split('\n').map((s, i) => (
            <p className={classes.bodyText} key={i}>
              {s}
            </p>
          ))}
        </span>
      </Typography>
      {
        showTOSDisclaimer
        && (
          <>
            <Typography variant='subtitle2' className={classes.disclaimer}>
              By signing up, you&apos;re agreeing to&nbsp;
              <a href='https://pixielabs.ai/terms/'>Terms of Service</a>
              &nbsp;and&nbsp;
              <a href='https://pixielabs.ai/privacy'>Privacy Policy</a>
              .
            </Typography>
          </>
        )
      }
      {children}
      <div className={classes.gutter}>
        <Typography variant='subtitle2' className={classes.account}>
          {buttonCaption}
        </Typography>
        <Button
          component={Link}
          color='primary'
          href={toggleURL}
          onClick={reportToggleClick}
          // eslint-disable-next-line react-memo/require-usememo
          sx={{ ml: buttonCaption ? 1 : 0 }}
        >
          {buttonText}
        </Button>
      </div>
      <div/>
    </PixienautBox>
  );
});
AuthBox.displayName = 'AuthBox';
