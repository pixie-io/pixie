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

import { Box, IconButton } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import Highlight, { defaultProps } from 'prism-react-renderer';

import { scrollbarStyles } from 'app/components';
import { CopyIcon } from 'app/components/icons/copy';

const useStyles = makeStyles((theme: Theme) => createStyles({
  code: {
    backgroundColor: theme.palette.foreground.grey3,
    borderRadius: theme.shape.borderRadius,
    boxShadow: '0px 6px 18px rgba(0, 0, 0, 0.0864292)',
    marginTop: theme.spacing(3),
    position: 'relative',
    padding: theme.spacing(1),
    paddingRight: theme.spacing(6.875), // 55px
    ...scrollbarStyles(theme),
  },
  codeHighlight: {
    display: 'block',
    width: '100%',
    overflowX: 'auto',
    fontFamily: theme.typography.monospace.fontFamily,
    marginLeft: '1rem',
  },
  copyBtn: {
    position: 'absolute',
    top: '50%',
    transform: 'translateY(-50%)',
    right: '0',
    cursor: 'pointer',
  },
}), { name: 'CodeRenderer' });

export const CodeRenderer: React.FC<{
  code: string,
  language?: React.ComponentProps<typeof Highlight>['language'],
}> = React.memo(({ code, language = 'javascript' }) => {
  const classes = useStyles();
  return (
    <div className={classes.code}>
      <Box className={`${classes.codeHighlight} small-scroll`}>
        <Highlight {...defaultProps} code={code.trim()} language={language}>
          {({
            className, style, tokens, getLineProps, getTokenProps,
          }) => (
            <pre
              className={className}
              style={{ ...style, backgroundColor: 'transparent' }}
            >
              {tokens.map((line, i) => (
                <div key={i} {...getLineProps({ line, key: i })}>
                  {line.map((token, key) => (
                    <span key={key} {...getTokenProps({ token, key })} />
                  ))}
                </div>
              ))}
            </pre>
          )}
        </Highlight>
      </Box>
      <IconButton
        edge='start'
        color='inherit'
        className={classes.copyBtn}
        onClick={React.useCallback(() => {
          navigator.clipboard.writeText(code).then();
        }, [code])}
      >
        <CopyIcon />
      </IconButton>
    </div>
  );
});
CodeRenderer.displayName = 'CodeRenderer';
