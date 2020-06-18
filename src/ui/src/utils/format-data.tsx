import './format-data.scss';

import clsx from 'clsx';
import * as numeral from 'numeral';
import * as React from 'react';
import { DataType } from 'types/generated/vizier_pb';

const JSON_INDENT_PX = 16;

const LATENCY_HIGH_THRESHOLD = 300;
const LATENCY_MEDIUM_THRESHOLD = 150;

interface JSONDataProps {
  data: any;
  indentation?: number;
  multiline?: boolean;
  className?: string;
}

export function formatInt64Data(val: string): string {
  return numeral(val).format('0,0');
}

export function formatFloat64Data(val: number, formatStr = '0[.]00'): string {
  // Numeral.js doesn't actually format NaNs, it ignores them.
  if (isNaN(val)) {
    return 'NaN';
  }

  let num = numeral(val).format(formatStr);
  // Numeral.js doesn't have a catch for abs-value decimals less than 1e-6.
  if (num === 'NaN' && Math.abs(val) < 1e-6) {
    num = formatFloat64Data(0);
  }
  return num;
}

export function looksLikeLatencyCol(colName: string, colType: DataType) {
  if (colType !== DataType.FLOAT64) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/latency.*/)) {
    return true;
  }
  if (colNameLC.match(/p\d{0,2}$/)) {
    return true;
  }
  return false;
}

export function looksLikeAlertCol(colName: string, colType: DataType) {
  if (colType !== DataType.BOOLEAN) {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/alert.*/)) {
    return true;
  }
  return false;
}

export const JSONData = React.memo<JSONDataProps>((props) => {
  const indentation = props.indentation ? props.indentation : 0;
  let data = props.data;
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
      <span className={clsx('formatted_data--json', props.className)}>
        {'[ '}
        {props.multiline ? <br /> : null}
        {
          data.map((val, idx) => {
            return (
              <span
                key={idx + '-' + indentation}
                style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
              >
                <JSONData data={val} multiline={props.multiline} indentation={indentation + 1} />
                {idx !== Object.keys(data).length - 1 ? ', ' : ''}
                {props.multiline ? <br /> : null}
              </span>
            );
          })
        }
        <span style={{ marginLeft: props.multiline ? indentation * JSON_INDENT_PX : 0 }}>{' ]'}</span>
      </span>);
  }

  if (typeof data === 'object' && data !== null) {
    return (
      <span className={clsx('formatted_data--json', props.className)}>
        {'{ '}
        {props.multiline ? <br /> : null}
        {
          Object.keys(data).map((key, idx) => {
            return (
              <span
                key={key + '-' + indentation}
                style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
              >
                <span className='formatted_data--json-key'>{key + ': '}</span>
                <JSONData data={data[key]} multiline={props.multiline} indentation={indentation + 1} />
                {idx !== Object.keys(data).length - 1 ? ', ' : ''}
                {props.multiline ? <br /> : null}
              </span>
            );
          })
        }
        <span style={{ marginLeft: props.multiline ? indentation * JSON_INDENT_PX : 0 }}>{' }'}</span>
      </span>);
  }
  return <span className={clsx(`formatted_data--json-${cls}`, props.className)}>{String(data)}</span>;
});
JSONData.displayName = 'JSONData';

export function LatencyData(data: string) {
  const floatVal = parseFloat(data);
  let latency = 'low';

  if (floatVal > LATENCY_HIGH_THRESHOLD) {
    latency = 'high';
  } else if (floatVal > LATENCY_MEDIUM_THRESHOLD) {
    latency = 'med';
  }
  return <div className={'formatted_data--latency-' + latency}>{data}</div>;
}

export function AlertData(data: string) {
  return <div className={'formatted_data--alert-' + data}>{data}</div>;
}

// Converts UInt128 to UUID formatted string.
export function formatUInt128(high: string, low: string): string {
  // TODO(zasgar/michelle): Revisit this to check and make sure endianness is correct.
  // Each segment of the UUID is a hex value of 16 nibbles.
  // Note: BigInt support only available in Chrome > 67, FF > 68.
  const hexStrHigh = BigInt(high).toString(16).padStart(16, '0');
  const hexStrLow = BigInt(low).toString(16).padStart(16, '0');

  // Sample UUID: 123e4567-e89b-12d3-a456-426655440000.
  // Format is 8-4-4-4-12.
  let uuidStr = '';
  uuidStr += hexStrHigh.substr(0, 8);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(8, 4);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(12, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(0, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(4);
  return uuidStr;
}

export function getDataRenderer(type: DataType) {
  switch (type) {
    case DataType.FLOAT64:
      return formatFloat64Data;
    case DataType.TIME64NS:
      return (d) => new Date(d).toLocaleString();
    case DataType.INT64:
      return formatInt64Data;
    case DataType.DURATION64NS:
    case DataType.UINT128:
    case DataType.STRING:
    case DataType.BOOLEAN:
    default:
      return (d) => d.toString();
  }
}
