import * as React from 'react';
import './format-data.scss';

const JSON_INDENT_PX = 16;

const LATENCY_HIGH_THRESHOLD = 300;
const LATENCY_MEDIUM_THRESHOLD = 150;

interface JSONDataProps {
  data: any;
  indentation?: number;
  multiline?: boolean;
}

export function looksLikeLatencyCol(colName: string, colType: string) {
  if (colType !== 'FLOAT64') {
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

export function looksLikeAlertCol(colName: string, colType: string) {
  if (colType !== 'BOOLEAN') {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/alert.*/)) {
    return true;
  }
  return false;
}

export function JSONData(props) {
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

  if (typeof data === 'object' && data !== null) {
    return (
      <span className='formatted_data--json'>
        {'{ '}
        {props.multiline ? <br/> : null}
        {
          Object.keys(data).map((key, idx) => {
            return (
              <span
                key={key + '-' + indentation}
                style={{ marginLeft: props.multiline ? (indentation + 1) * JSON_INDENT_PX : 0 }}
              >
                <span className='formatted_data--json-key'>{key + ': ' }</span>
                <JSONData  data={data[key]} multiline={props.multiline} indentation={indentation + 1}/>
                { idx !== Object.keys(data).length - 1 ? ', ' : ''}
                {props.multiline ? <br/> : null}
              </span>
            );
          })
        }
        <span style={{ marginLeft: props.multiline ? indentation * JSON_INDENT_PX : 0 }}>{' }'}</span>
      </span>);
  }
  return <span className={'formatted_data--json-' + cls}>{String(data)}</span>;
}

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
