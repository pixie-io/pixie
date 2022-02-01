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
  Brightness1 as UnknownIcon,
  CheckCircle as HealthyIcon,
  Error as UnhealthyIcon,
  WatchLater as PendingIcon,
  Warning as WarningIcon,
} from '@mui/icons-material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

export type StatusGroup = 'healthy' | 'unhealthy' | 'pending' | 'unknown' | 'degraded';

const useStyles = makeStyles(({ palette }: Theme) => createStyles({
  unhealthy: {
    verticalAlign: 'text-bottom',
    color: palette.error.main,
  },
  healthy: {
    verticalAlign: 'text-bottom',
    color: palette.success.main,
  },
  pending: {
    verticalAlign: 'text-bottom',
    color: palette.warning.main,
  },
  warning: {
    verticalAlign: 'text-bottom',
    color: palette.warning.main,
  },
  unknown: {
    verticalAlign: 'text-bottom',
    color: palette.foreground.grey1,
  },
}), { name: 'StatusCell' });

export const StatusCell: React.FC<{ statusGroup: StatusGroup }> = React.memo(({ statusGroup }) => {
  const classes = useStyles();
  switch (statusGroup) {
    case 'healthy':
      return <HealthyIcon fontSize='small' className={classes.healthy} />;
    case 'unhealthy':
      return <UnhealthyIcon fontSize='small' className={classes.unhealthy} />;
    case 'pending':
      return <PendingIcon fontSize='small' className={classes.pending} />;
    case 'degraded':
      return <WarningIcon fontSize='small' className={classes.warning} />;
    default:
      return <UnknownIcon fontSize='small' className={classes.unknown} />;
  }
});
StatusCell.displayName = 'StatusCell';
