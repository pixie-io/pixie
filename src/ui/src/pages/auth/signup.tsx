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

import {
  Theme, WithStyles, withStyles,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import * as React from 'react';
import { AuthBox, SignupMarcom } from 'app/components';
import { BasePage } from './base';
import { GetOAuthProvider } from './utils';

const styles = ({ breakpoints }: Theme) => createStyles({
  root: {
    width: '100%',
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-evenly',
    alignItems: 'center',
  },
  marketingBlurb: {
    [breakpoints.down('sm')]: {
      display: 'none',
    },
  },
});

export const SignupPage = withStyles(styles)(({ classes }: WithStyles<typeof styles>) => {
  const authClient = React.useMemo(() => GetOAuthProvider(), []);
  const buttons = React.useMemo(
    () => (authClient.getSignupButtons()),
    [authClient]);
  return (
    <BasePage>
      <div className={classes.root}>
        <div className={classes.marketingBlurb}>
          <SignupMarcom />
        </div>
        <div>
          <AuthBox
            toggleURL={`/auth/login${window.location.search}`}
            title='Get Started'
            // Need to encapsulate so that newline is properly escaped.
            body={`Pixie Community is Free Forever.
          No Credit Card Needed.`}
            buttonCaption='Already have an account?'
            buttonText='Login'
            showTOSDisclaimer
          >
            {buttons}
          </AuthBox>
        </div>
      </div>
    </BasePage>
  );
});
