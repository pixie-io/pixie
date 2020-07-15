import * as React from 'react';
import clsx from 'clsx';
import { createStyles, Theme, withStyles } from '@material-ui/core/styles';
import { formatBoolData, formatFloat64Data } from 'utils/format-data';
import { GaugeLevel, getCPULevel, getLatencyLevel } from 'utils/metric-thresholds';

const JSON_INDENT_PX = 16;

export const AlertDataBase = ({ data, classes }) => (
  <div className={classes[`${data}`]}>{formatBoolData(data)}</div>
);

export const AlertData = withStyles((theme: Theme) => ({
  true: {
    color: theme.palette.error.dark,
  },
  false: {},
}))(AlertDataBase);

interface JSONDataProps {
  data: any;
  indentation?: number;
  multiline?: boolean;
  classes: any;
}

const jsonStyles = (theme: Theme) => createStyles({
  base: {
    fontFamily: '"Roboto Mono", serif',
    fontSize: 13,
  },
  jsonKey: {
    color: theme.palette.foreground?.white,
  },
  number: {
    color: '#ae81ff',
  },
  null: {
    color: '#f92672',
  },
  string: {
    color: theme.palette.info.light,
    wordBreak: 'break-all',
  },
  boolean: {
    color: '#f92672',
  },
});

const JSONBase = React.memo<JSONDataProps>((props) => {
  const indentation = props.indentation ? props.indentation : 0;
  const { classes, multiline } = props;
  let { data } = props;
  let cls = String(typeof data);

  if (cls === 'string') {
    try {
      const parsedJson = JSON.parse(data);
      data = parsedJson;
    } catch {
      // Do nothing.
    }
  }

  if (data === null) {
    cls = 'null';
  }

  if (Array.isArray(data)) {
    return (
      <span className={classes.base}>
        {'[ '}
        {props.multiline ? <br /> : null}
        {
          data.map((val, idx) => (
            <span
              key={`${idx}-${indentation}`}
              style={{ marginLeft: multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <JSONData data={val} multiline={multiline} indentation={indentation + 1} />
              {idx !== Object.keys(data).length - 1 ? ', ' : ''}
              {multiline ? <br /> : null}
            </span>
          ))
        }
        <span style={{ marginLeft: multiline ? indentation * JSON_INDENT_PX : 0 }}>{' ]'}</span>
      </span>
    );
  }

  if (typeof data === 'object' && data !== null) {
    return (
      <span className={classes.base}>
        {'{ '}
        {props.multiline ? <br /> : null}
        {
          Object.keys(data).map((key, idx) => (
            <span
              key={`${key}-${indentation}`}
              style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <span className={classes.jsonKey}>{`${key}: `}</span>
              <JSONData data={data[key]} multiline={props.multiline} indentation={indentation + 1} />
              {idx !== Object.keys(data).length - 1 ? ', ' : ''}
              {props.multiline ? <br /> : null}
            </span>
          ))
        }
        <span style={{ marginLeft: multiline ? indentation * JSON_INDENT_PX : 0 }}>{' }'}</span>
      </span>
    );
  }
  return <span className={classes[cls]}>{String(data)}</span>;
});
// linter needs this for React.memo components.
JSONBase.displayName = 'JSONBase';

export const JSONData = withStyles(jsonStyles, {
  name: 'JSONData',
})(JSONBase);

interface GaugeDataProps {
  classes: any;
  data: any;
  getLevel: (number) => GaugeLevel;
}

const GaugeDataBase = ({ getLevel, classes, data }: GaugeDataProps) => {
  const floatVal = parseFloat(data);
  return (
    <div className={classes[getLevel(floatVal)]}>
      {formatFloat64Data(floatVal)}
    </div>
  );
};

const gaugeStyles = (theme: Theme) => createStyles({
  low: {
    color: theme.palette.success.dark,
  },
  med: {
    color: theme.palette.warning.dark,
  },
  high: {
    color: theme.palette.error.dark,
  },
});

export const LatencyData = withStyles(gaugeStyles, {
  name: 'LatencyData',
})(({ classes, data }: any) => (
  <GaugeDataBase classes={classes} data={data} getLevel={getLatencyLevel} />
));

export const CPUData = withStyles(gaugeStyles, {
  name: 'CPUData',
})(({ classes, data }: any) => (
  <GaugeDataBase classes={classes} data={data} getLevel={getCPULevel} />
));
