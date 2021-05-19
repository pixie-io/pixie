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
  alpha,
  Theme,
  WithStyles,
  withStyles,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import { PixienautBalloonSvg } from './pixienaut-balloon';
import { PixienautOctopusSvg } from './pixienaut-octopus';
import { PixienautToiletSvg } from './pixienaut-toilet';

const styles = ({ spacing, palette, breakpoints }: Theme) => createStyles({
  root: {
    backgroundColor: alpha(palette.foreground.grey3, 0.8),
    paddingLeft: spacing(5),
    paddingRight: spacing(5),
    paddingTop: spacing(0),
    paddingBottom: spacing(1),
    boxShadow: `0px ${spacing(0.25)}px ${spacing(2)}px rgba(0, 0, 0, 0.6)`,
    borderRadius: spacing(3),
    minWidth: '370px',
    maxWidth: breakpoints.values.xs,
    maxHeight: '550px',
  },
  splashImageContainer: {
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'flex-start',
    padding: spacing(3),
  },
  pixienautBalloonContainer: {
    // The Pixienaut is still kinda close to Earth (this component's content)
    // This + splash container padding + relative Pixienaut position = spacing(4) under its foot
    marginBottom: spacing(-9),
    marginTop: spacing(-2),
  },
  pixienautBalloonImage: {
    // The balloons raise the Pixienaut up out of the container
    // (and correcting for unusual image dimensions)
    position: 'relative',
    bottom: spacing(8.5),
    left: spacing(2),
    padding: 0,
  },
  pixienautOctopusContainer: {
    width: '100%',
    marginBottom: spacing(-13),
  },
  pixienautOctopusImage: {
    maxWidth: '100%',
    height: 'auto',
    position: 'relative',
    bottom: spacing(13),
  },
  pixienautToiletContainer: {
    width: '100%',
    textAlign: 'center',
    marginBottom: spacing(-15),
  },
  pixienautToiletImage: {
    maxWidth: '60%',
    height: 'auto',
    position: 'relative',
    bottom: spacing(15),
  },
  content: {
    display: 'flex',
    flexFlow: 'column nowrap',
    alignItems: 'center',
    justifyContent: 'flex-start',
    width: '100%',
    textAlign: 'center',
  },
});

export type PixienautImage = 'balloon' | 'octopus' | 'toilet';

export interface PixienautBoxProps extends WithStyles<typeof styles> {
  children?: React.ReactNode;
  /**
   * What is the Pixienaut doing? Options:
   * - Being lifted by balloons (default)
   * - Running from a space octopus (use for recoverable errors)
   * - Using the toilet with an "oh no" pose (use for fatal errors)
   */
  image?: PixienautImage;
}

export const PixienautBox = withStyles(styles)(({ classes, children, image = 'balloon' }: PixienautBoxProps) => {
  const pixienautScenarios = {
    balloon: (
      <div className={classes.pixienautBalloonContainer}>
        <PixienautBalloonSvg className={classes.pixienautBalloonImage} />
      </div>
    ),
    octopus: (
      <div className={classes.pixienautOctopusContainer}>
        <PixienautOctopusSvg className={classes.pixienautOctopusImage} />
      </div>
    ),
    toilet: (
      <div className={classes.pixienautToiletContainer}>
        <PixienautToiletSvg className={classes.pixienautToiletImage} />
      </div>
    ),
  };
  return (
    <div className={classes.root}>
      <div className={classes.splashImageContainer}>
        {pixienautScenarios[image]}
      </div>
      <div className={classes.content}>
        {children}
      </div>
    </div>
  );
});
