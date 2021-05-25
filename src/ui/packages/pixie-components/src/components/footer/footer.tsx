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
import { Theme, Typography, makeStyles } from '@material-ui/core';
import { createStyles } from '@material-ui/styles';

const useDefaultStyles = makeStyles(({ spacing, breakpoints, palette }: Theme) => createStyles({
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
    textDecoration: 'none',
    fontSize: '0.875rem', // 14px
  },
}));

export interface FooterProps {
  copyright: React.ReactNode,
  /** Defaults to a layout with Terms & Conditions and Privacy Policy to the left; copyright to the right. */
  classes?: ReturnType<typeof useDefaultStyles>,
}

export const Footer: React.FC<FooterProps> = ({
  classes: overrideClasses,
  copyright,
}) => {
  const defaultClasses = useDefaultStyles();
  const classes = overrideClasses ?? defaultClasses;

  return (
    <div className={classes.root}>
      <div className={classes.left}>
        <a href='https://pixielabs.ai/terms/' className={classes.text}>
          Terms & Conditions
        </a>
        <a href='https://pixielabs.ai/privacy' className={classes.text}>
          Privacy Policy
        </a>
      </div>
      <div className={classes.right}>
        <Typography variant='subtitle2' className={classes.text}>
          {copyright}
        </Typography>
      </div>
    </div>
  );
};
