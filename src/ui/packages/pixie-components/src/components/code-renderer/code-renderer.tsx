import Highlight, { defaultProps } from 'prism-react-renderer';
import * as React from 'react';
import { CopyIcon } from 'components/icons/copy';

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
