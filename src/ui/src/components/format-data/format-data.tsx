import * as React from 'react';
import clsx from 'clsx';
import { formatBoolData, formatFloat64Data } from '../../utils/format-data';
import { getLatencyLevel } from './latency';

const JSON_INDENT_PX = 16;

interface JSONDataProps {
  data: any;
  indentation?: number;
  multiline?: boolean;
  className?: string;
}

export const JSONData = React.memo<JSONDataProps>((props) => {
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
      <span className={clsx('formatted_data--json', props.className)}>
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
      <span className={clsx('formatted_data--json', props.className)}>
        {'{ '}
        {props.multiline ? <br /> : null}
        {
          Object.keys(data).map((key, idx) => (
            <span
              key={`${key}-${indentation}`}
              style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
            >
              <span className='formatted_data--json-key'>{`${key}: `}</span>
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
  return <span className={clsx(`formatted_data--json-${cls}`, props.className)}>{String(data)}</span>;
});
JSONData.displayName = 'JSONData';

export function LatencyData(data: string) {
  const floatVal = parseFloat(data);
  const latency = getLatencyLevel(floatVal);
  return <div className={`formatted_data--latency-${latency}`}>{formatFloat64Data(floatVal)}</div>;
}

export function AlertData(data: boolean) {
  return <div className={`formatted_data--alert-${data}`}>{formatBoolData(data)}</div>;
}
