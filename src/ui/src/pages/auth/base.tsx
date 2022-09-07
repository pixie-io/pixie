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

// This contains the base template all auth pages. We really only allow inserting the
// middle part of the page which has either a dialog, or marcom information.

import * as React from 'react';

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { Footer } from 'app/components';
import { WithChildren } from 'app/utils/react-boilerplate';
import { Copyright } from 'configurable/copyright';
import { Logo } from 'configurable/logo';

import StarsPNG from './stars.png';

const useStyles = makeStyles(({ spacing, breakpoints }: Theme) => createStyles({
  root: {
    minHeight: breakpoints.values.xs,
    height: '100vh',
    minWidth: spacing(50), // 400px
    width: '100vw',
    overflow: 'auto',
    backgroundImage: `url(${StarsPNG})`,
    display: 'flex',
    flexFlow: 'column nowrap',
    justifyContent: 'space-between',
    alignItems: 'stretch',
  },
  content: {
    flex: '1 0 auto',
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  logo: {
    flex: '0 0 auto',
    paddingLeft: spacing(4),
    paddingTop: spacing(4),
    paddingRight: spacing(4),
    paddingBottom: spacing(1.0),
    // Includes vertical padding. spacing(4.5) is given to the image within.
    // Combined with footer height, this perfectly (vertically) centers the box containing the Pixienaut (sans balloons)
    height: spacing(9.5),
    width: '100%',
    [breakpoints.down('sm')]: {
      textAlign: 'center',
      paddingBottom: spacing(11.5), // Clear the balloons that break out of their container
      height: spacing(20), // Account for said clearance
    },
    '& > img': {
      height: '100%',
      width: 'auto',
    },
  },
}), { name: 'AuthPage' });

export const BasePage: React.FC<WithChildren> = React.memo(({ children }) => {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <div className={classes.logo}>
        <Logo />
      </div>
      <div className={classes.content}>
        {children}
      </div>
      <Footer copyright={Copyright} />
    </div>
  );
});
BasePage.displayName = 'AuthBasePage';
