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
import { Link } from 'react-router-dom';

import { VersionInfo } from 'app/components/version-info/version-info';
import { TermsAndPrivacy } from 'configurable/tos-privacy';

const useDefaultStyles = makeStyles(({ spacing, typography, breakpoints, palette }: Theme) => createStyles({
  root: {
    display: 'flex',
    flexFlow: 'row wrap',
    justifyContent: 'space-between',
    alignItems: 'flex-end',
    width: '100%',
    margin: 0,
    marginTop: spacing(3),
    [breakpoints.only('xs')]: {
      justifyContent: 'center',
    },

    '& > *': {
      display: 'flex',
      flexFlow: 'row wrap',
      paddingBottom: spacing(3),
      [breakpoints.only('xs')]: {
        justifyContent: 'center',
      },
    },
  },
  left: {
    justifyContent: 'flex-start',
  },
  right: {
    justifyContent: 'flex-end',
  },
  text: {
    padding: `0 ${spacing(3)}`,
    color: palette.foreground.three,
    fontSize: typography.body2.fontSize,
  },
}), { name: 'Footer' });

export interface FooterProps {
  copyright: React.ComponentType<Record<string, never>>,
  /** Defaults to a layout with Terms & Conditions and Privacy Policy to the left; copyright to the right. */
  classes?: ReturnType<typeof useDefaultStyles>,
}

export const Footer = React.memo<FooterProps>(({
  classes: overrideClasses,
  copyright: Copyright,
}) => {
  const defaultClasses = useDefaultStyles();
  const classes = overrideClasses ?? defaultClasses;

  return (
    <div className={classes.root}>
      <div className={classes.left}>
        <TermsAndPrivacy classes={classes} />
        <div className={classes.text}><VersionInfo /></div>
      </div>
      <div className={classes.right}>
        <Link to='/credits' className={classes.text}>Credits</Link>
        <Typography variant='subtitle2' className={classes.text}>
          <Copyright />
        </Typography>
      </div>
    </div>
  );
});
Footer.displayName = 'Footer';
