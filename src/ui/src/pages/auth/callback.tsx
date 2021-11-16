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

import { Button, ButtonProps } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { AuthMessageBox } from 'app/components';
import pixieAnalytics from 'app/utils/analytics';
import { isValidAnalytics } from 'app/utils/env';
import * as RedirectUtils from 'app/utils/redirect-utils';
import Axios, { AxiosError } from 'axios';
import * as QueryString from 'query-string';
import * as React from 'react';
import { Link } from 'react-router-dom';
import { BasePage } from './base';
import { Token } from './oauth-provider';
import { AuthCallbackMode, GetOAuthProvider } from './utils';

// Send token header to enable CORS check. Token is still allowed with Pixie CLI.
const redirectGet = async (url: string, data: { accessToken: string }) => (
  Axios.get(url, { headers: { token: data.accessToken } })
);

type ErrorType = 'internal' | 'auth';

interface ErrorDetails {
  errorType?: ErrorType;
  errMessage?: string;
}

interface CallbackConfig {
  mode?: AuthCallbackMode;
  signup?: boolean;
  token?: string;
  loading?: boolean;
  err?: ErrorDetails;
  isEmailUnverified?: boolean;
}

function trackAuthEvent(event: string, id: string, email: string): Promise<void> {
  if (isValidAnalytics()) {
    return Promise.race([
      new Promise<void>((resolve) => { // Wait for analytics to be sent out before redirecting.
        pixieAnalytics.track(event, () => {
          pixieAnalytics.identify(id, { email }, {}, () => {
            resolve();
          });
        });
      }),
      // Wait a maximum of 4s before redirecting. If it takes this long, it probably means that
      // something in Segment failed to initialize/send.
      new Promise((resolve) => setTimeout(resolve, 4000)),
    ]).then();
  }

  return Promise.resolve();
}

function getCtaDetails(config: CallbackConfig) {
  let ctaMessage: string;
  let ctaDestination: string;
  let errorMessage: string;
  let showDetails = true;
  if (config.err.errorType === 'internal') {
    if (config.signup) {
      if (config.err.errMessage.match(/user.*already.*exists/ig)) {
        errorMessage = 'Account already exists. Please login.';
        ctaMessage = 'Go to Log In';
        ctaDestination = '/auth/login';
        showDetails = false;
      } else {
        errorMessage = 'We hit a snag in creating an account. Please try again later.';
        ctaMessage = 'Back to Sign Up';
        ctaDestination = '/auth/signup';
      }
    } else if (config.err.errMessage.match(/.*organization.*not.*found.*/g)
      || config.err.errMessage.match(/.*user.*not.*found.*/g)) {
      errorMessage = 'We hit a snag trying to authenticate you. User not registered. Please sign up.';
      // If user or organization not found, direct to sign up page first.
      ctaMessage = 'Go to Sign Up';
      ctaDestination = '/auth/signup';
    } else if (config.err.errMessage.match(/verify.*your.*email/ig)) {
      errorMessage = 'Please verify your email before logging in.';
      ctaMessage = 'Go to Log In';
      ctaDestination = '/auth/login';
      showDetails = false;
    } else {
      errorMessage = 'We hit a snag trying to authenticate you. Please try again later.';
      ctaMessage = 'Back to Log In';
      ctaDestination = '/auth/login';
    }
  } else {
    errorMessage = config.err.errMessage;
    ctaMessage = 'Go Back';
    ctaDestination = '/';
  }
  return {
    ctaMessage,
    ctaDestination,
    errorMessage,
    showDetails,
  };
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  ctaGutter: {
    marginTop: theme.spacing(3),
    paddingTop: theme.spacing(3),
    borderTop: `1px solid ${theme.palette.foreground.grey1}`,
    width: '80%',
  },
}));

const CLICodeBox = React.memo<{ code: string }>(function CLICodeBox({ code }) {
  return (
    <AuthMessageBox
      title='Pixie Auth Token'
      message='Please copy this code, switch to the CLI and paste it there:'
      code={code}
    />
  );
});

const CtaButton = React.memo<ButtonProps>(function CtaButton({ children, ...props }) {
  return <Button color='primary' variant='contained' {...props}>{children}</Button>;
});

const UnverifiedEmailMessage = React.memo(function UnverifiedEmailMessage() {
  const classes = useStyles();
  const ctaMessage = 'Back to Sign Up';
  const ctaDestination = '/auth/signup';
  const cta = React.useMemo(() => (
    <div className={classes.ctaGutter}>
      <Link to={ctaDestination} component={CtaButton}>
        {ctaMessage}
      </Link>
    </div>
  ), [classes.ctaGutter]);

  return (
    <AuthMessageBox
      error='recoverable'
      title='Check your inbox to finish signup.'
      message='We sent a signup link to your email.'
      cta={cta}
    />
  );
});

const ErrorMessage = React.memo<{ config: CallbackConfig }>(function ErrorMessage({ config }) {
  const classes = useStyles();
  const title = config.signup ? 'Failed to Sign Up' : 'Failed to Log In';
  const errorDetails = config.err.errorType === 'internal' ? config.err.errMessage : undefined;

  const {
    ctaMessage,
    ctaDestination,
    errorMessage,
    showDetails,
  } = getCtaDetails(config);

  const cta = React.useMemo(() => (
    <div className={classes.ctaGutter}>
      <Link to={ctaDestination} component={CtaButton}>
        {ctaMessage}
      </Link>
    </div>
  ), [classes.ctaGutter, ctaDestination, ctaMessage]);

  return (
    <AuthMessageBox
      error='recoverable'
      title={title}
      message={errorMessage}
      errorDetails={showDetails ? errorDetails : ''}
      cta={cta}
    />
  );
});

/**
 * This is the main component to handle the callback from auth.
 *
 * This component gets the token from Auth0 and either sends it to the CLI or
 * makes a request to Pixie cloud to perform a signup/login.
 */
export const AuthCallbackPage: React.FC = React.memo(function AuthCallbackPage() {
  const [config, setConfig] = React.useState<CallbackConfig>(null);

  const setErr = React.useCallback((errType: ErrorType, errMsg: string) => {
    setConfig((c) => ({
      ...c,
      err: {
        errorType: errType,
        errMessage: errMsg,
      },
      loading: false,
    }));
  }, []);

  const handleHTTPError = React.useCallback((err: AxiosError) => {
    if (err.code === '401' || err.code === '403' || err.code === '404') {
      setErr('auth', err.response.data);
    } else {
      setErr('internal', err.response.data);
    }
  }, [setErr]);

  const performSignup = React.useCallback(async (accessToken: string, idToken: string) => {
    let response = null;
    try {
      response = await Axios.post('/api/auth/signup', { accessToken, idToken });
    } catch (err) {
      pixieAnalytics.track('User signup failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
    await trackAuthEvent('User signed up', response.data.userInfo.userID, response.data.userInfo.email);
    return true;
  }, [handleHTTPError]);

  const performUILogin = React.useCallback(async (accessToken: string, idToken: string, orgName: string) => {
    let response = null;
    try {
      response = await Axios.post('/api/auth/login', {
        accessToken,
        idToken,
        orgName,
      });
    } catch (err) {
      pixieAnalytics.track('User login failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
    await trackAuthEvent('User logged in', response.data.userInfo.userID, response.data.userInfo.email);
    return true;
  }, [handleHTTPError]);

  const sendTokenToCLI = React.useCallback(async (accessToken: string, idToken: string, redirectURI: string) => {
    try {
      const response = await redirectGet(redirectURI, { accessToken });
      return response.status === 200 && response.data === 'OK';
    } catch (error) {
      handleHTTPError(error as AxiosError);
      // If there's an error, we just return a failure.
      return false;
    }
  }, [handleHTTPError]);

  const doAuth = React.useCallback(async (
    mode: AuthCallbackMode,
    signup: boolean,
    redirectURI: string,
    location: string,
    orgName: string,
    accessToken: string,
    idToken: string,
  ) => {
    let signupSuccess = false;
    let loginSuccess = false;

    if (signup) {
      // We always need to perform signup, even if the mode is CLI.
      signupSuccess = await performSignup(accessToken, idToken);
    }
    // eslint-disable-next-line default-case
    switch (mode) {
      case 'cli_get':
        loginSuccess = await sendTokenToCLI(accessToken, idToken, redirectURI);
        if (loginSuccess) {
          setConfig((c) => ({
            ...c,
            loading: false,
          }));
          RedirectUtils.redirect('/auth/cli-auth-complete', {});
          return;
        }

        // Don't fallback to manual auth if there is an actual
        // authentication error.
        if (config.err?.errorType === 'auth') {
          break;
        }

        // If it fails, switch to token auth.
        setConfig((c) => ({
          ...c,
          mode: 'cli_token',
        }));
        break;
      case 'cli_token':
        // Nothing to do, it will just render.
        break;
      case 'ui':
        if (!signup) {
          loginSuccess = await performUILogin(accessToken, idToken, orgName);
        }
        // We just need to redirect if in signup or login were successful since
        // the cookies are installed.
        if ((signup && signupSuccess) || loginSuccess) {
          RedirectUtils.redirect(redirectURI || location || '/', {});
        }
    }

    setConfig((c) => ({
      ...c,
      loading: false,
    }));
  }, [config?.err?.errorType, performSignup, performUILogin, sendTokenToCLI]);

  const handleAccessToken = React.useCallback((token: Token) => {
    const params = QueryString.parse(window.location.search.substr(1));
    let mode: AuthCallbackMode = 'ui';
    switch (params.mode) {
      case 'cli_get':
      case 'cli_token':
      case 'ui':
        ({ mode } = params);
        break;
      default:
        break;
    }

    const location = params.location && String(params.location);
    const signup = !!params.signup;
    const orgName = params.org_name && String(params.org_name);
    const redirectURI = params.redirect_uri && String(params.redirect_uri);

    setConfig({
      mode,
      signup,
      token: token?.accessToken,
      loading: true,
      isEmailUnverified: token.isEmailUnverified,
    });

    if (!token?.accessToken) {
      setConfig({
        mode,
        signup,
        token: token?.accessToken,
        loading: false,
        isEmailUnverified: token.isEmailUnverified,
      });
      return;
    }

    doAuth(mode, signup, redirectURI, location, orgName, token?.accessToken, token?.idToken).then();
  }, [doAuth]);

  React.useEffect(() => {
    GetOAuthProvider().handleToken().then(handleAccessToken).catch((err) => {
      setErr('internal', `${err}`);
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const loadingMessage = React.useMemo(() => (
    <AuthMessageBox
      title='Authenticating'
      message={
        (config && config.signup) ? 'Signing up ...' : 'Logging in...'
          || '...'
      }
    />
  ), [config]);

  const normalMessage = React.useMemo(() => {
    if (config?.isEmailUnverified) {
      return <UnverifiedEmailMessage />;
    }

    if (config?.mode === 'cli_token') {
      return (
        <CLICodeBox
          code={config.token}
        />
      );
    }

    return loadingMessage;
  }, [config?.isEmailUnverified, config?.mode, config?.token, loadingMessage]);

  const loading = !config || config.loading;
  return (
    <BasePage>
      {loading && loadingMessage}
      {!loading && (config.err ? <ErrorMessage config={config} /> : normalMessage)}
    </BasePage>
  );
});
