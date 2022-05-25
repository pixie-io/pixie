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

import { Typography, alpha, Link } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { PixienautBox } from 'app/components';

import { WithChildren } from './react-boilerplate';

export interface ErrorBoundaryFallbackProps {
  error: Error;
  /** React.ErrorInfo.componentStack, if available. */
  info?: string;
}

export interface ErrorBoundaryProps {
  name: string;
  fallback?: React.ComponentType<ErrorBoundaryFallbackProps>;
}

interface ErrorBoundaryState {
  error: Error | null;
  errorInfo: React.ErrorInfo | null;
}

/**
 * General purpose way to render something other than a blank white screen if a component throws an error.
 * Wrap this around any tree that should show something meaningful in place of said crash. This prevents errors from
 * bubbling up any higher than their relevant context as well, preserving parts of the app that didn't break.
 */
export class ErrorBoundary extends React.PureComponent<WithChildren<ErrorBoundaryProps>, ErrorBoundaryState> {
  static readonly displayName = 'ErrorBoundary';

  constructor(props: WithChildren<ErrorBoundaryProps>) {
    super(props);
    this.state = { error: null, errorInfo: null };
  }

  static getDerivedStateFromError(error: Error): Partial<ErrorBoundaryState> {
    return { error };
  }

  // If children change, reset error state so they can attempt to render.
  componentDidUpdate(prevProps: WithChildren<ErrorBoundaryProps>): void {
    if (prevProps.children !== this.props.children) {
      this.setState({ error: null, errorInfo: null });
    }
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo): void {
    // Keep the component stack for a more useful error message when presented to the user.
    this.setState({ error, errorInfo });

    // NOTE: https://reactjs.org/docs/react-component.html#componentdidcatch -- if this method exists, then a production
    // build of React will NOT bubble the error up to the window. We need it to anyway, so that analytics (if enabled)
    // can log the problem. A development build of React already lets the event bubble up.
    if (process.env.NODE_ENV !== 'development') {
      window.dispatchEvent(new ErrorEvent('error', { error }));
    }
  }

  render(): React.ReactNode {
    if (this.state.error) {
      if (this.props.fallback) {
        return <this.props.fallback error={this.state.error} info={this.state.errorInfo?.componentStack} />;
      }
      // In case the error happened so far up that not even useTheme and <PixienautBox> are available...
      return (
        <>
          <h1>Pixie&apos;s UI encountered a fatal error. Check browser console for details.</h1>
          <p>
            {'You can check reports '}
            <a target='_blank' rel='noreferrer' href='https://github.com/pixie-io/pixie/issues'>on GitHub</a>
            {', or file a new issue.'}
          </p>
          <pre>{this.state.error.toString()}</pre>
        </>
      );
    }
    return this.props.children;
  }
}

const useErrorStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    width: '100%',
    height: '100%',
    maxHeight: '100%',
    overflow: 'auto',
  },
  errorBox: {
    backgroundColor: alpha(theme.palette.error.dark, 0.6),
    border: `1px ${theme.palette.error.main} solid`,
    borderRadius: theme.spacing(1),
    color: theme.palette.common.white,
    padding: theme.spacing(1),
    maxWidth: '100%',
    maxHeight: theme.spacing(25),
    overflow: 'auto',
    textAlign: 'left',
  },
}), { name: 'PixienautCrashFallback' });

/**
 * An alternative state to show when a component crashes (throws an uncaught error).
 * Takes up the page with a Pixienaut graphic, the error, and a message explaining possible remedies.
 */
export const PixienautCrashFallback = React.memo<ErrorBoundaryFallbackProps>(({ error, info }) => {
  const classes = useErrorStyles();

  return (
    <div className={classes.root}>
      <PixienautBox image='toilet'>
        <Typography variant='h1'>UI Crashed!</Typography>
        <Typography variant='body1'>Pixie&apos;s UI hit an unrecoverable snag.</Typography>
        <Typography variant='body1'>This is almost certainly a bug.</Typography>
        <Typography variant='body1'>
          {'You can check reports '}
          <Link target='_blank' rel='noreferrer' href='https://github.com/pixie-io/pixie/issues'>on GitHub</Link>
          {', or file a new issue.'}
        </Typography>
        {error && (
          <pre className={classes.errorBox}>
            {error.message}
            {info}
            {info && (
              <>
                <br/><br/>
                {'Raw error:'}
                <br/>
              </>
            )}
            {error.stack.split('\n').map((l) => `  at ${l}`).join('\n')}
          </pre>
        )}
      </PixienautBox>
    </div>
  );
});
PixienautCrashFallback.displayName = 'PixienautCrashFallback';
