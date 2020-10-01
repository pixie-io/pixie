import * as React from 'react';
import { createStyles, Theme, withStyles } from '@material-ui/core/styles';
import { formatBoolData, formatFloat64Data, formatUInt128Protobuf } from 'utils/format-data';
import {
  GaugeLevel,
  getCPULevel,
  getLatencyLevel,
  getLatencyNSLevel,
} from 'utils/metric-thresholds';
import { DataType, SemanticType } from '../../types/generated/vizier_pb';

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
    fontSize: '14px',
  },
  jsonKey: {
    color: theme.palette.foreground?.white,
  },
  number: {
    color: theme.palette.secondary.main,
  },
  null: {
    color: theme.palette.success.main,
  },
  string: {
    color: theme.palette.info.light,
    wordBreak: 'break-all',
  },
  boolean: {
    color: theme.palette.success.main,
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
        {'[\u00A0'}
        {props.multiline ? <br /> : null}
        {
          data.map((val, idx) => (
            <span
              key={`${idx}-${indentation}`}
              style={{ marginLeft: multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <JSONData data={val} multiline={multiline} indentation={indentation + 1} />
              {idx !== Object.keys(data).length - 1 ? ',\u00A0' : ''}
              {multiline ? <br /> : null}
            </span>
          ))
        }
        <span style={{ marginLeft: multiline ? indentation * JSON_INDENT_PX : 0 }}>{'\u00A0]'}</span>
      </span>
    );
  }

  if (typeof data === 'object' && data !== null) {
    return (
      <span className={classes.base}>
        {'{\u00A0'}
        {props.multiline ? <br /> : null}
        {
          Object.keys(data).map((key, idx) => (
            <span
              key={`${key}-${indentation}`}
              style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <span className={classes.jsonKey}>
                {`${key}:\u00A0`}
              </span>
              <JSONData data={data[key]} multiline={props.multiline} indentation={indentation + 1} />
              {idx !== Object.keys(data).length - 1 ? ',\u00A0' : ''}
              {props.multiline ? <br /> : null}
            </span>
          ))
        }
        <span style={{ marginLeft: multiline ? indentation * JSON_INDENT_PX : 0 }}>{'\u00A0}'}</span>
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

export const GaugeData = withStyles(gaugeStyles, {
  name: 'GaugeData',
})(({ classes, data, level }: any) => (
  <div className={classes[level]}>
    {data}
  </div>
));

export const CPUData = withStyles(gaugeStyles, {
  name: 'CPUData',
})(({ classes, data }: any) => (
  <GaugeDataBase classes={classes} data={data} getLevel={getCPULevel} />
));

export const PortRendererBase = ({ data, classes }) => (
  <>
    <span className={classes.value}>{data}</span>
  </>
);

export const PortRenderer = withStyles(() => ({
  value: {
    fontFamily: '"Roboto Mono", serif',
    fontSize: '14px',
  },
}))(PortRendererBase);

export interface DataWithUnits {
  val: string;
  units: string;
}

export const formatBytes = (data: number): DataWithUnits => {
  const decimals = 2;
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['\u00a0B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];

  const i = Math.min(Math.floor(Math.log(data) / Math.log(k)), sizes.length + 1);

  const val = `${parseFloat((data / (k ** i)).toFixed(dm))}\u00A0`;
  const units = sizes[i];

  return {
    val,
    units,
  };
};

export const formatDuration = (data: number): DataWithUnits => {
  const decimals = 2;
  const k = 1000;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['ns', '\u00b5s', 'ms', '\u00a0s'];

  const i = Math.min(Math.floor(Math.log(data) / Math.log(k)), sizes.length + 1);

  const val = `${parseFloat((data / (k ** i)).toFixed(dm))}\u00A0`;
  const units = sizes[i];

  return {
    val,
    units,
  };
};

const RenderValueWithUnitsBase = ({ data, classes }: {data: DataWithUnits; classes: any}) => (
  <>
    <span className={classes.value}>{data.val}</span>
    <span className={classes.units}>{data.units}</span>
  </>
);

const RenderValueWithUnits = withStyles(() => ({
  units: {
    opacity: 0.5,
    fontFamily: '"Roboto Mono", serif',
    fontSize: '14px',
  },
  value: {
    fontFamily: '"Roboto Mono", serif',
    fontSize: '14px',
  },
}))(RenderValueWithUnitsBase);

export const BytesRenderer = ({ data }: {data: number}) => <RenderValueWithUnits data={formatBytes(data)} />;
export const DurationRenderer = ({ data }: {data: number}) => (
  <GaugeData
    data={<RenderValueWithUnits data={formatDuration(data)} />}
    level={getLatencyNSLevel(data)}
  />
);

export const formatBySemType = (semType: SemanticType, val: any): DataWithUnits => {
  switch (semType) {
    case SemanticType.ST_BYTES:
      return formatBytes(val);
    case SemanticType.ST_DURATION_NS:
      return formatDuration(val);
    default:
      break;
  }

  return {
    val,
    units: '',
  };
};

export const formatByDataType = (dataType: DataType, val: any): string => {
  switch (dataType) {
    case DataType.FLOAT64:
      return formatFloat64Data(val);
    case DataType.BOOLEAN:
      return formatBoolData(val);
    case DataType.UINT128:
      return formatUInt128Protobuf(val);
    case DataType.STRING:
    default:
      return val;
  }
};
