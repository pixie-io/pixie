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

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { StatusCell } from 'app/components';
import { GQLClusterInfo, GQLClusterStatus } from 'app/types/schema';
import { GaugeLevel } from 'app/utils/metric-thresholds';

import {
  AdminTooltip, clusterStatusGroup, StyledLeftTableCell, StyledTableCell,
} from './utils';

function getPercentInstrumentedLevel(instrumentedRatio: number): GaugeLevel {
  if (instrumentedRatio > 0.9) {
    return 'high';
  }
  if (instrumentedRatio > 0.6) {
    return 'med';
  }
  return 'low';
}

interface InstrumentationLevelDisplay {
  percentInstrumented: string;
  percentInstrumentedLevel: GaugeLevel;
}

type InstrumentationClusterInfo = Pick<
GQLClusterInfo,
'status' |
'numNodes' |
'numInstrumentedNodes'
>;

function instrumentationLevel(clusterInfo: InstrumentationClusterInfo): InstrumentationLevelDisplay {
  let percentInstrumented = 'N/A';
  let percentInstrumentedLevel: GaugeLevel = 'none';
  if (clusterInfo.status !== 'CS_DISCONNECTED') {
    const percent = clusterInfo.numNodes ? clusterInfo.numInstrumentedNodes / clusterInfo.numNodes : 0;
    const formattedPercent = clusterInfo.numNodes ? `${(percent * 100).toFixed(0)}%` : 'N/A';
    percentInstrumented = `${formattedPercent} (${clusterInfo.numInstrumentedNodes} of ${clusterInfo.numNodes})`;
    percentInstrumentedLevel = clusterInfo.numNodes ? getPercentInstrumentedLevel(percent) : 'low';
  }
  return {
    percentInstrumented,
    percentInstrumentedLevel,
  };
}

const useInstrumentationLevelStyles = makeStyles((theme: Theme) => createStyles({
  low: {
    color: theme.palette.error.main,
  },
  med: {
    color: theme.palette.warning.main,
  },
  high: {
    color: theme.palette.success.main,
  },
}));

// eslint-disable-next-line react-memo/require-memo
export const InstrumentationLevelCell: React.FC<{ cluster: InstrumentationClusterInfo }> = ({ cluster }) => {
  const classes = useInstrumentationLevelStyles();
  const display = instrumentationLevel(cluster);
  return (<StyledTableCell className={classes[display.percentInstrumentedLevel]}>
    {display.percentInstrumented}
  </StyledTableCell>);
};

const useClusterStatusCellStyle = makeStyles((theme: Theme) => createStyles({
  status: {
    display: 'inline-block',
  },
  message: {
    paddingLeft: theme.spacing(1),
    display: 'inline-block',
  },
}));

// eslint-disable-next-line react-memo/require-memo
export const ClusterStatusCell: React.FC<{ status: GQLClusterStatus, message?: string }> = ({ status, message }) => {
  const classes = useClusterStatusCellStyle();

  return (<AdminTooltip title={status.replace('CS_', '')}>
    <StyledLeftTableCell>
      <div className={classes.status}>
        <StatusCell statusGroup={clusterStatusGroup(status)} />
      </div>
      {message && <div className={classes.message}>{message}</div>}
    </StyledLeftTableCell>
  </AdminTooltip>
  );
};

// eslint-disable-next-line react-memo/require-memo
export const VizierVersionCell: React.FC<{ version: string }> = ({ version }) => (
  <AdminTooltip title={version}>
    <StyledTableCell>{version.split('+')[0]}</StyledTableCell>
  </AdminTooltip>
);

const useMonoCells = makeStyles(() => createStyles({
  mono: {
    fontFamily: '"Roboto Mono", Monospace',
  },
}));

// MonoSpaceCell renders the in monospace.
// eslint-disable-next-line react-memo/require-memo
export const MonoSpaceCell: React.FC<{ data: string }> = ({ data }) => {
  const classes = useMonoCells();
  return (
    <StyledTableCell className={classes.mono}>{data}</StyledTableCell>
  );
};
