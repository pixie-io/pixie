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
import * as numeral from 'numeral';

import { ResultsContext } from 'app/context/results-context';

function nanoSecDisplay(ns: number) {
  const ms = ns / 1e6;
  if (ms >= 0.1) {
    return `${numeral(ms).format('0[.]00')} ms`;
  }
  return `${numeral(ns).format('0[.]00')} ns`;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    height: '100%',
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-around',
    overflow: 'auto',
  },
  metric: {
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
  },
  name: {
    ...theme.typography.h3,
  },
  value: {
    ...theme.typography.h2,
  },
}), { name: 'ExecutionStats' });

interface ExecutionMetricProps {
  metricName: string;
  metricValue: string;
}

// eslint-disable-next-line react-memo/require-memo
const ExecutionMetric = (props: ExecutionMetricProps) => {
  const classes = useStyles();
  return (
    <div className={classes.metric}>
      <div className={classes.value}>
        {props.metricValue}
      </div>
      <div className={classes.name}>
        {props.metricName}
      </div>
    </div>
  );
};
ExecutionMetric.displayName = 'ExecutionMetric';

// eslint-disable-next-line react-memo/require-memo
const ExecutionStats: React.FC = () => {
  const { stats } = React.useContext(ResultsContext);
  const classes = useStyles();

  if (!stats) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'row',
        height: '100%',
        alignItems: 'center',
        justifyContent: 'center',
      }}
      >
        Execute script to see execution stats
      </div>
    );
  }

  const numBytes = numeral(stats.getBytesProcessed()).format('0[.]00b');
  const numRecords = numeral(stats.getRecordsProcessed()).format('0[.]00a');
  const executionTime = nanoSecDisplay(stats.getTiming().getExecutionTimeNs());
  const compilationTime = nanoSecDisplay(stats.getTiming().getCompilationTimeNs());

  return (
    <div className={classes.root}>
      <ExecutionMetric metricName='Bytes Processed' metricValue={numBytes} />
      <ExecutionMetric metricName='Records Processed' metricValue={numRecords} />
      <ExecutionMetric metricName='Execution Time' metricValue={executionTime} />
      <ExecutionMetric metricName='Compilation Time' metricValue={compilationTime} />
    </div>
  );
};
ExecutionStats.displayName = 'ExecutionStats';

export default ExecutionStats;
