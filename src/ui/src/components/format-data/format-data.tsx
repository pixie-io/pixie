import * as React from 'react';
import clsx from 'clsx';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { formatBoolData, formatFloat64Data } from 'utils/format-data';
import { getCPULevel, getLatencyLevel } from 'utils/metric-thresholds';

const JSON_INDENT_PX = 16;

const useAlertStyles = makeStyles((theme: Theme) => ({
  true: {
    color: theme.palette.error.dark,
  },
  false: {},
}));

const useGaugeStyles = makeStyles((theme: Theme) => ({
  low: {
    color: theme.palette.success.dark,
  },
  med: {
    color: theme.palette.warning.dark,
  },
  high: {
    color: theme.palette.error.dark,
  },
}));

const useJSONStyles = makeStyles((theme: Theme) => ({
  base: {
    fontFamily: '"Roboto Mono", serif',
    fontSize: 13,
  },
  jsonKey: {
    color: theme.palette.foreground.white,
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
}));

interface JSONDataProps {
  data: any;
  indentation?: number;
  multiline?: boolean;
  className?: string;
}

export const JSONData = React.memo<JSONDataProps>((props) => {
  const classes = useJSONStyles();
  const indentation = props.indentation ? props.indentation : 0;
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
              style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <JSONData data={val} multiline={props.multiline} indentation={indentation + 1} />
              {idx !== Object.keys(data).length - 1 ? ', ' : ''}
              {props.multiline ? <br /> : null}
            </span>
          ))
        }
        <span style={{ marginLeft: props.multiline ? indentation * JSON_INDENT_PX : 0 }}>{' ]'}</span>
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
        <span style={{ marginLeft: props.multiline ? indentation * JSON_INDENT_PX : 0 }}>{' }'}</span>
      </span>
    );
  }
  return <span className={props.className || classes[cls]}>{String(data)}</span>;
});
JSONData.displayName = 'JSONData';

export const LatencyData = ({ data }) => {
  const classes = useGaugeStyles();
  const floatVal = parseFloat(data);
  const level = getLatencyLevel(floatVal);
  return <div className={classes[level]}>{formatFloat64Data(floatVal)}</div>;
};

export const CPUData = ({ data }) => {
  const classes = useGaugeStyles();
  const floatVal = parseFloat(data);
  const level = getCPULevel(floatVal);
  return <div className={classes[level]}>{formatFloat64Data(floatVal)}</div>;
};

export const AlertData = ({ data }) => {
  const classes = useAlertStyles();
  return <div className={classes[`${data}`]}>{formatBoolData(data)}</div>;
};
