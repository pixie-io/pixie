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

import Highlight, { defaultProps } from 'prism-react-renderer';
import * as React from 'react';
import { CopyIcon } from 'app/components/icons/copy';

import { scrollbarStyles } from 'app/components';
import { Box } from '@material-ui/core';
import { Theme } from '@material-ui/core/styles';
import IconButton from '@material-ui/core/IconButton';
import withStyles from '@material-ui/core/styles/withStyles';

export const CodeRenderer = withStyles((theme: Theme) => ({
  code: {
    backgroundColor: theme.palette.foreground.grey3,
    borderRadius: '5px',
    boxShadow: '0px 6px 18px rgba(0, 0, 0, 0.0864292)',
    marginTop: '24px',
    position: 'relative',
    padding: '8px 55px 8px 8px',
    ...scrollbarStyles(theme),
  },

  codeHighlight: {
    display: 'block',
    width: '100%',
    overflowX: 'auto',
    fontFamily: '"Roboto Mono", Monospace',
    marginLeft: '1rem',
  },

  copyBtn: {
    position: 'absolute',
    top: '50%',
    transform: 'translateY(-50%)',
    right: '0',
    cursor: 'pointer',
  },
}))(({ classes, code, language = 'javascript' }: any) => (
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
      onClick={() => {
        navigator.clipboard.writeText(code);
      }}
    >
      <CopyIcon />
    </IconButton>
  </div>
));
