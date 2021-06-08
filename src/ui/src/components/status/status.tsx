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

import UnknownIcon from '@material-ui/icons/Brightness1';
import HealthyIcon from '@material-ui/icons/CheckCircle';
import UnhealthyIcon from '@material-ui/icons/Error';
import PendingIcon from '@material-ui/icons/WatchLater';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import * as React from 'react';

export type StatusGroup = 'healthy' | 'unhealthy' | 'pending' | 'unknown';

const useStyles = makeStyles((theme: Theme) => createStyles({
  unhealthy: {
    verticalAlign: 'text-bottom',
    color: theme.palette.error.main,
  },
  healthy: {
    verticalAlign: 'text-bottom',
    color: theme.palette.success.main,
  },
  pending: {
    verticalAlign: 'text-bottom',
    color: theme.palette.warning.main,
  },
  unknown: {
    verticalAlign: 'text-bottom',
    color: theme.palette.foreground.grey1,
  },
}));

export const StatusCell: React.FC<{ statusGroup: StatusGroup }> = ({ statusGroup }) => {
  const classes = useStyles();
  switch (statusGroup) {
    case 'healthy':
      return <HealthyIcon fontSize='small' className={classes.healthy} />;
    case 'unhealthy':
      return <UnhealthyIcon fontSize='small' className={classes.unhealthy} />;
    case 'pending':
      return <PendingIcon fontSize='small' className={classes.pending} />;
    default:
      return <UnknownIcon fontSize='small' className={classes.unknown} />;
  }
};
