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
  Grid,
  Typography,
  Container,
  alpha,
} from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { CPPIcon } from 'app/components/logos/cpp';
import { GolangIcon } from 'app/components/logos/golang';
import { HeartIcon } from 'app/components/logos/heart';
import { RustIcon } from 'app/components/logos/rust';
import { Logo } from 'configurable/logo';

const useStyles = makeStyles(({ palette, spacing }: Theme) => createStyles({
  heading: {
    color: palette.foreground.two,
    textAlign: 'center',
  },
  message: {
    color: palette.primary.light,
    textAlign: 'center',
  },
  pixieLove: {
    display: 'flex',
    flexFlow: 'row nowrap',
    gap: spacing(2),
    marginTop: spacing(5),
    alignItems: 'center',
    background: `linear-gradient(180deg, ${alpha(
      palette.background.four,
      0.87,
    )},
    ${alpha(palette.background.five, 0.22)})`,
    boxShadow: `2px 2px 2px 0px ${palette.background.default}`,
    paddingLeft: spacing(2),
    paddingRight: spacing(2),
    height: spacing(4),
  },
  pixieLoveItem: {
    height: spacing(3),
    display: 'flex',
    alignItems: 'center',
  },
  logoItem: {
    height: spacing(1.25),
    marginRight: spacing(-1.5), // Offset gap between this item and the heart
  },
}), { name: 'SignupMarcom' });

export const SignupMarcom = React.memo(() => {
  const classes = useStyles();
  return (
    <Container maxWidth='sm'>
      <Grid
        container
        direction='column'
        spacing={4}
        justifyContent='flex-start'
        alignItems='center'
      >
        <Grid item>
          <Typography variant='h1' className={classes.heading}>
            Instantly troubleshoot your applications on Kubernetes
          </Typography>
        </Grid>
        <Grid item>
          <Typography variant='subtitle1' className={classes.message}>
            NO CODE CHANGES. NO MANUAL INTERFACES. ALL INSIDE K8S.
          </Typography>
        </Grid>
        <Grid item>
          <div className={classes.pixieLove}>
            <div className={classes.logoItem}>
              <Logo color='white' />
            </div>
            <div className={classes.pixieLoveItem}>
              <HeartIcon />
            </div>
            <div className={classes.pixieLoveItem}>
              <GolangIcon />
            </div>
            <div className={classes.pixieLoveItem}>
              <CPPIcon />
            </div>
            <div className={classes.pixieLoveItem}>
              <RustIcon />
            </div>
          </div>
        </Grid>
      </Grid>
    </Container>
  );
});
SignupMarcom.displayName = 'SignupMarcom';
