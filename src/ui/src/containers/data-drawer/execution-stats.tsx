import { ResultsContext } from 'context/results-context';
import * as numeral from 'numeral';
import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

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
    ...theme.typography.subtitle2,
  },
  value: {
    ...theme.typography.h4,
  },
}));

const ExecutionStats = () => {
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

interface ExecutionMetricProps {
  metricName: string;
  metricValue: string;
}

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

export default ExecutionStats;
