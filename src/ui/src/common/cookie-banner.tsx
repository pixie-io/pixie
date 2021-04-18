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
  createStyles, makeStyles, Theme,
} from '@material-ui/core/styles';
import * as React from 'react';

import CookieBanner from 'react-cookie-banner';

const useStyles = makeStyles((theme: Theme) => createStyles({
  container: {
    position: 'absolute',
    bottom: 0,
    width: '100%',
    backgroundColor: theme.palette.background.three,
    color: theme.palette.text.secondary,
    opacity: 0.9,
    zIndex: 1400,
    '& > .react-cookie-banner': {
      alignItems: 'center',
      display: 'flex',
      justifyContent: 'center',
      padding: theme.spacing(1),
    },
    '& .button-close': {
      textTransform: 'none',
      marginLeft: theme.spacing(3),
      backgroundColor: theme.palette.background.three,
      border: 'solid 1px',
      borderRadius: theme.spacing(0.5),
      padding: theme.spacing(0.8),
      paddingRight: theme.spacing(1),
      paddingLeft: theme.spacing(1),
      fontWeight: 500,
      color: theme.palette.foreground.grey5,
      cursor: 'pointer',
      '&:hover': {
        color: theme.palette.foreground.white,
      },
    },
  },
  link: {
    color: theme.palette.primary.main,
    '&:visited': {
      color: theme.palette.primary.main,
    },
  },
}));

export default function PixieCookieBanner() {
  const classes = useStyles();
  const cookieLink = (
    <>
      <a className={classes.link} href='https://pixielabs.ai/privacy#Cookies'>use of cookies</a>
      .
    </>
  );

  return (
    <div className={classes.container}>
      <CookieBanner
        link={cookieLink}
        disableStyle
        message='This site uses cookies to provide you with a better user experience. By using Pixie,
          you consent to our '
        cookie='user-has-accepted-cookies'
      />
    </div>
  );
}
