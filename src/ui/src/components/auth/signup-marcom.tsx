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
  WithStyles,
  withStyles,
  Theme,
  Typography,
  Container,
  alpha,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';

import { CPPIcon } from 'app/components/logos/cpp';
import { GolangIcon } from 'app/components/logos/golang';
import { HeartIcon } from 'app/components/logos/heart';
import { PixieLogo } from 'app/components/icons/pixie-logo';
import { RustIcon } from 'app/components/logos/rust';

const styles = ({ palette, spacing }: Theme) => createStyles({
  root: {
    backgroundColor: palette.background.default,
  },
  heading: {
    color: palette.foreground.white,
    textAlign: 'center',
  },
  message: {
    color: palette.primary.light,
    textAlign: 'center',
  },
  pixieLove: {
    display: 'flex',
    marginTop: spacing(5),
    alignItems: 'center',
    background: `linear-gradient(180deg, ${alpha(
      palette.background.two,
      0.87,
    )},
    ${alpha(palette.background.three, 0.22)})`,
    boxShadow: `2px 2px 2px 0px ${palette.background.default}`,
    paddingLeft: spacing(2),
    paddingRight: spacing(2),
  },
  pixieLoveItem: {
    display: 'flex',
    paddingLeft: spacing(1),
    paddingRight: spacing(1),
  },
});

export const SignupMarcom = withStyles(styles)(
  ({ classes }: WithStyles<typeof styles>) => (
    <>
      <Container maxWidth='sm' className={classes.root}>
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
              <PixieLogo fontSize='large' />
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
    </>
  ),
);
