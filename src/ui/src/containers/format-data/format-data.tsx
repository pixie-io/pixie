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
  Theme,
  WithStyles,
  withStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { formatBoolData, formatFloat64Data, formatUInt128Protobuf } from 'utils/format-data';
import {
  GaugeLevel,
  getCPULevel,
  getLatencyNSLevel,
} from 'utils/metric-thresholds';
import { buildClass } from '@pixie-labs/components';
import { DataType, SemanticType } from 'types/generated/vizierapi_pb';

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

/* eslint-disable @typescript-eslint/no-use-before-define */
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

  const splits = String(data)
    .replace(/ /g, '\u00A0')
    .split('\n');

  const indent = {
    marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0,
  };

  return (
    <span className={classes[cls]}>
      {
        splits.map((val, idx) => (
          <span key={idx} style={idx > 0 ? indent : {}}>
            {val}
            {props.multiline && (idx !== splits.length - 1) ? <br /> : null}
          </span>
        ))
      }
    </span>
  );
});
/* eslint-enable @typescript-eslint/no-use-before-define */
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

export const formatScaled = (data: number, scale: number, suffixes: string[], decimals = 2): DataWithUnits => {
  const dm = decimals < 0 ? 0 : decimals;
  let i = Math.floor(Math.log(Math.abs(data)) / Math.log(scale));
  i = Math.max(0, Math.min(i, suffixes.length - 1));

  const val = `${parseFloat((data / (scale ** i)).toFixed(dm))}`;
  const units = suffixes[i];

  return {
    val,
    units,
  };
};

export const formatBytes = (data: number): DataWithUnits => (
  formatScaled(data,
    1024,
    ['\u00a0B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],
    1)
);

export const formatDuration = (data: number): DataWithUnits => (
  formatScaled(data,
    1000,
    ['ns', '\u00b5s', 'ms', '\u00a0s'],
    1)
);

export const formatThroughput = (data: number): DataWithUnits => (
  formatScaled(data * 1E9,
    1000,
    ['\u00a0/s', 'K/s', 'M/s', 'B/s'],
    1)
);

export const formatThroughputBytes = (data: number): DataWithUnits => (
  formatScaled(data * 1E9,
    1024,
    ['\u00a0B/s', 'KB/s', 'MB/s', 'GB/s'],
    1)
);

const RenderValueWithUnitsBase = ({ data, classes }: { data: DataWithUnits; classes: any }) => (
  <>
    <span className={classes.value}>{`${data.val}\u00A0`}</span>
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

export const BytesRenderer = ({ data }: { data: number }) => <RenderValueWithUnits data={formatBytes(data)} />;
export const DurationRenderer = ({ data }: { data: number }) => (
  <GaugeData
    data={<RenderValueWithUnits data={formatDuration(data)} />}
    level={getLatencyNSLevel(data)}
  />
);

const httpStatusCodeRendererStyles = (theme: Theme) => createStyles({
  root: {},
  unknown: {
    color: theme.palette.foreground.grey1,
  },
  oneHundredLevel: {
    color: theme.palette.success.main,
  },
  twoHundredLevel: {
    color: theme.palette.success.main,
  },
  threeHundredLevel: {
    color: theme.palette.success.main,
  },
  fourHundredLevel: {
    color: theme.palette.error.main,
  },
  fiveHundredLevel: {
    color: theme.palette.error.main,
  },
});

interface HTTPStatusCodeRendererProps extends WithStyles<typeof httpStatusCodeRendererStyles> {
  data: string;
}

export const HTTPStatusCodeRenderer = withStyles(httpStatusCodeRendererStyles)(
  ({ classes, data }: HTTPStatusCodeRendererProps) => {
    const intVal = parseInt(data, 10);
    const cls = buildClass(
      classes.root,
      intVal < 0 && classes.unknown,
      intVal > 0 && intVal < 200 && classes.oneHundredLevel,
      intVal >= 200 && intVal < 300 && classes.twoHundredLevel,
      intVal >= 300 && intVal < 400 && classes.threeHundredLevel,
      intVal >= 400 && intVal < 500 && classes.fourHundredLevel,
      intVal >= 500 && classes.fiveHundredLevel);

    return (
      <>
        <span className={cls}>{data}</span>
      </>
    );
  });

export const formatPercent = (data: number): DataWithUnits => {
  const val = (100 * parseFloat(data.toString())).toFixed(1);
  const units = '%';

  return {
    val,
    units,
  };
};

interface PercentRendererProps {
  data: number;
}

export const PercentRenderer = ({ data }: PercentRendererProps) => <RenderValueWithUnits data={formatPercent(data)} />;

export const ThroughputRenderer = ({ data }: PercentRendererProps) => (
  <RenderValueWithUnits data={formatThroughput(data)} />
);

export const ThroughputBytesRenderer = ({ data }: PercentRendererProps) => (
  <RenderValueWithUnits data={formatThroughputBytes(data)} />
);

// FormatFnMetadata stores a function with the string rep of that function.
// We need this datastructure to support formatting in Vega Axes.
export interface FormatFnMetadata {
  name: string;
  formatFn: (data: number) => DataWithUnits;
}

export const getFormatFnMetadata = (semType: SemanticType): FormatFnMetadata => {
  switch (semType) {
    case SemanticType.ST_BYTES:
      return { formatFn: formatBytes, name: 'formatBytes' };
    case SemanticType.ST_DURATION_NS:
      return { formatFn: formatDuration, name: 'formatDuration' };
    case SemanticType.ST_PERCENT:
      return { formatFn: formatPercent, name: 'formatPercent' };
    case SemanticType.ST_THROUGHPUT_PER_NS:
      return { formatFn: formatThroughput, name: 'formatThroughput' };
    case SemanticType.ST_THROUGHPUT_BYTES_PER_NS:
      return { formatFn: formatThroughputBytes, name: 'formatThroughputBytes' };
    default:
      break;
  }
  return null;
};

export const formatBySemType = (semType: SemanticType, val: any): DataWithUnits => {
  const formatFnMd = getFormatFnMetadata(semType);
  if (formatFnMd) {
    return formatFnMd.formatFn(val);
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
