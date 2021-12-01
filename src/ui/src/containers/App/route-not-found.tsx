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

import { Button, Typography } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useLocation } from 'react-router';

import { PixienautBox } from 'app/components';

const useStyles = makeStyles(({ palette, spacing }: Theme) => createStyles({
  title: {
    color: palette.foreground.two,
  },
  message: {
    color: palette.foreground.one,
    marginTop: spacing(4),
    marginBottom: spacing(4),
  },
  footer: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-around',
    alignItems: 'center',
    paddingTop: spacing(4),
    paddingBottom: spacing(4),
    borderTop: `1px solid ${palette.foreground.grey1}`,
    width: '81%', // Lines up with the border under the octopus graphic and puts space between the buttons
  },
}), { name: 'RouteNotFound' });

// eslint-disable-next-line react-memo/require-memo
export const RouteNotFound: React.FC = () => {
  const styles = useStyles();
  const { pathname } = useLocation();
  return (
    <PixienautBox image='octopus'>
      <Typography variant='h1' className={styles.title}>
        404 Not Found
      </Typography>
      <p className={styles.message}>
        {/* eslint-disable-next-line react/jsx-one-expression-per-line */}
        The route &quot;<code>{pathname}</code>&quot; doesn&apos;t exist.
      </p>
      <div className={styles.footer}>
        <Button href='/logout' variant='text'>Log Out</Button>
        <Button href='/live' variant='contained' color='primary'>Live View</Button>
      </div>
    </PixienautBox>
  );
};
RouteNotFound.displayName = 'RouteNotFound';
