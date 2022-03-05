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

import { buildClass } from 'app/utils/build-class';
import { checkExhaustive } from 'app/utils/check-exhaustive';

const useStyles = makeStyles(({ palette, spacing, typography }: Theme) => createStyles({
  root: {
    display: 'inline', // Ensures single-line mode honors text-overflow:ellipsis if its parent specifies as much.
    fontFamily: typography.monospace.fontFamily,
    fontSize: typography.body2.fontSize,
    '&$multiline': {
      lineHeight: spacing(3),
      '& $closure': {
        display: 'block',
        marginLeft: spacing(2),
      },
      '& $entry': {
        display: 'block',
      },
      '& $openBrace, & $closeBrace, & $comma': {
        paddingLeft: 0,
        paddingRight: 0,
      },
    },
  },
  multiline: {/* Empty; used in nested rules */},
  closure: {/* Empty; used in nested rules */},
  entry: {/* Empty; used in nested rules */},
  openBrace: {
    display: 'inline-block',
    paddingRight: spacing(1),
  },
  closeBrace: {
    display: 'inline-block',
    paddingLeft: spacing(1),
  },
  comma: {
    display: 'inline-block',
    paddingRight: spacing(1),
  },
  jsonKey: {
    color: palette.syntax.normal,
  },
  number: {
    color: palette.syntax.number,
  },
  null: {
    color: palette.syntax.nullish,
  },
  undefined: {
    color: palette.syntax.nullish,
  },
  string: {
    color: palette.syntax.string,
    wordBreak: 'break-all',
  },
  boolean: {
    color: palette.syntax.boolean,
  },
  error: {
    color: palette.syntax.error,
    fontWeight: 'bold',
  },
}), { name: 'JSONData' });

const JSONObject = React.memo<{ data: Record<string, any> }>(({ data }) => {
  const classes = useStyles();
  const entries = Object.entries(data);
  return (
    <>
      <span className={classes.openBrace}>{'{'}</span>
      {entries.length > 0 && (
        <span className={classes.closure}>
          {entries.map(([key, value], index) => (
            <span className={classes.entry} key={key}>
              <span className={classes.jsonKey}>{key}:&nbsp;</span>
              {/* eslint-disable-next-line @typescript-eslint/no-use-before-define */}
              <JSONInner data={value} />
              {index < entries.length - 1 && (<span className={classes.comma}>,</span>)}
            </span>
          ))}
        </span>
      )}
      <span className={classes.closeBrace}>{'}'}</span>
    </>
  );
});
JSONObject.displayName = 'JSONObject';

const JSONArray = React.memo<{ data: any[] }>(({ data }) => {
  const classes = useStyles();
  return (
    <>
      <span className={classes.openBrace}>{'['}</span>
      {data.length > 0 && (
        <span className={classes.closure}>
          {data.map((value, index) => (
            <span key={index} className={classes.entry}>
              {/* eslint-disable-next-line @typescript-eslint/no-use-before-define */}
              <JSONInner data={value} />
              {index < data.length - 1 && (<span className={classes.comma}>,</span>)}
            </span>
          ))}
        </span>
      )}
      <span className={classes.closeBrace}>{']'}</span>
    </>
  );
});
JSONArray.displayName = 'JSONArray';

// eslint-disable-next-line consistent-return
const JSONInner = React.memo<{ data: any }>(({ data }) => {
  const classes = useStyles();
  if (data === undefined) {
    return <span className={classes.undefined}>undefined</span>;
  }
  if (data === null) {
    return <span className={classes.null}>null</span>;
  }
  const kind = typeof data;
  switch (kind) {
    case 'number':
    case 'boolean':
      return <span className={classes[kind]}>{String(data)}</span>;
    case 'string': {
      try {
        const parsed = JSON.parse(data);
        if (parsed && typeof parsed === 'object') {
          return <JSONInner data={parsed} />;
        }
      } catch { /* No-op */ }
      if (!data.length) {
        return <span className={classes.undefined}>(empty string)</span>;
      }
      return <span className={classes.string}>{data}</span>;
    }
    case 'object': {
      if (Array.isArray(data)) {
        return <JSONArray data={data} />;
      }
      return <JSONObject data={data} />;
    }
    case 'bigint':
    case 'symbol':
    case 'function':
    case 'undefined':
      return <span className={classes.error}>[ERR: Unexpected data type: {kind}]</span>;
    default:
      checkExhaustive(kind);
      break;
  }
});
JSONInner.displayName = 'JSONInner';

export const JSONData: React.FC<{
  data: any,
  multiline?: boolean,
}> = React.memo(({ data, multiline = false }) => {
  const classes = useStyles();
  return (
    <div className={buildClass(classes.root, multiline && classes.multiline)}>
      <JSONInner data={data} />
    </div>
  );
});
JSONData.displayName = 'JSONData';
