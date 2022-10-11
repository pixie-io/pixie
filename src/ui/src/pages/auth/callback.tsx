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

import { Button, ButtonProps } from '@mui/material';
import { styled } from '@mui/material/styles';
import Axios, { AxiosError } from 'axios';
import * as QueryString from 'query-string';
import { Link } from 'react-router-dom';

import { AuthMessageBox } from 'app/components';
import pixieAnalytics from 'app/utils/analytics';
import { isValidAnalytics } from 'app/utils/env';
import * as RedirectUtils from 'app/utils/redirect-utils';

import { BasePage } from './base';
import { AuthCallbackMode, CallbackArgs } from './callback-url';
import { GetOAuthProvider } from './utils';

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
  let title: string;
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
      title = 'Please verify your email to continue';
      errorMessage = 'Check your email for a verification link.';
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
    title,
    ctaMessage,
    ctaDestination,
    errorMessage,
    showDetails,
  };
}

// eslint-disable-next-line react-memo/require-memo
const CtaContainer = styled('div')(({ theme }) => ({
  marginTop: theme.spacing(3),
  paddingTop: theme.spacing(3),
  borderTop: `1px solid ${theme.palette.foreground.grey1}`,
  width: '80%',
}));

const CLICodeBox = React.memo<{ code: string }>(({ code }) => (
  <AuthMessageBox
    title='Pixie Auth Token'
    message='Please copy this code, switch to the CLI and paste it there:'
    code={code}
  />
));
CLICodeBox.displayName = 'CLICodeBox';

const CtaButton = React.memo<ButtonProps>(({ children, ...props }) => (
  <Button color='primary' variant='contained' {...props}>{children}</Button>
));
CtaButton.displayName = 'CtaButton';

const ErrorMessage = React.memo<{ config: CallbackConfig }>(({ config }) => {
  const title = config.signup ? 'Failed to Sign Up' : 'Failed to Log In';
  const errorDetails = config.err.errorType === 'internal' ? config.err.errMessage : undefined;

  const {
    title: ctaTitle,
    ctaMessage,
    ctaDestination,
    errorMessage,
    showDetails,
  } = getCtaDetails(config);

  const cta = React.useMemo(() => (
    <CtaContainer>
      <Link to={ctaDestination} component={CtaButton}>
        {ctaMessage}
      </Link>
    </CtaContainer>
  ), [ctaDestination, ctaMessage]);

  return (
    <AuthMessageBox
      error='recoverable'
      title={ctaTitle || title}
      message={errorMessage}
      errorDetails={showDetails ? errorDetails : ''}
      cta={cta}
    />
  );
});
ErrorMessage.displayName = 'ErrorMessage';

const checkOrg = (response: any, accessToken: string, mode: AuthCallbackMode, redirectURI: string) => {
  if (response.data.orgInfo && response.data.orgInfo.orgName !== '') {
    return true;
  }
  if (mode !== 'cli_get' && mode !== 'cli_token') {
    return true;
  }

  const postSetupParams = { token: accessToken, mode, redirect: redirectURI };
  const postSetupRedirect = `/auth/cli-token?${QueryString.stringify(postSetupParams)}`;
  const setupParams = { redirect_uri: postSetupRedirect };
  const setupURI = `/setup?${QueryString.stringify(setupParams)}`;

  RedirectUtils.redirect(setupURI, {});
  return false;
};

/**
 * This is the main component to handle the callback from auth.
 *
 * This component gets the token from Auth0 and either sends it to the CLI or
 * makes a request to Pixie cloud to perform a signup/login.
 */
export const AuthCallbackPage: React.FC = React.memo(() => {
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

  const performSignup = React.useCallback(async (
    accessToken: string,
    idToken: string,
    inviteToken: string,
    mode: AuthCallbackMode,
    redirectURI: string,
  ) => {
    let response = null;
    try {
      response = await Axios.post('/api/auth/signup', { accessToken, idToken, inviteToken });
    } catch (err) {
      pixieAnalytics.track('User signup failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
    await trackAuthEvent('User signed up', response.data.userInfo.userID, response.data.userInfo.email);
    return checkOrg(response, accessToken, mode, redirectURI);
  }, [handleHTTPError]);

  const performUILogin = React.useCallback(async (
    accessToken: string,
    idToken: string,
    inviteToken: string,
    mode: AuthCallbackMode,
    redirectURI: string,
  ) => {
    let response = null;
    try {
      response = await Axios.post('/api/auth/login', {
        accessToken,
        idToken,
        inviteToken,
      });
    } catch (err) {
      pixieAnalytics.track('User login failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
    await trackAuthEvent('User logged in', response.data.userInfo.userID, response.data.userInfo.email);
    return checkOrg(response, accessToken, mode, redirectURI);
  }, [handleHTTPError]);

  const sendTokenToCLI = React.useCallback(async (accessToken: string, idToken: string, redirectURI: string) => {
    try {
      // Check the URL is from an accepted origin.
      const parsedURL = new URL(redirectURI);
      if (parsedURL.hostname != 'localhost') {
        return false;
      }

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
    accessToken: string,
    idToken: string,
    inviteToken: string,
  ) => {
    const loginFunc = signup ? performSignup : performUILogin;
    const loginSuccess = await loginFunc(accessToken, idToken, inviteToken, mode, redirectURI);

    // eslint-disable-next-line default-case
    switch (mode) {
      case 'cli_get':
        if (loginSuccess) {
          const cliSuccess = await sendTokenToCLI(accessToken, idToken, redirectURI);
          if (cliSuccess) {
            setConfig((c) => ({
              ...c,
              loading: false,
            }));
            RedirectUtils.redirect('/auth/cli-auth-complete', {});
            return;
          }
          // Fallback to manual auth unless there is an actual authentication error.
          if (!config || config.err?.errorType !== 'auth') {
            setConfig((c) => ({
              ...c,
              mode: 'cli_token',
            }));
          }
        }
        break;
      case 'cli_token':
        // Nothing to do, it will just render.
        break;
      case 'ui':
        // We just need to redirect if in signup or login were successful since
        // the cookies are installed.
        if (loginSuccess) {
          RedirectUtils.redirect(redirectURI || '/', {});
        }
    }

    setConfig((c) => ({
      ...c,
      loading: false,
    }));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [config?.err?.errorType, performSignup, performUILogin, sendTokenToCLI]);

  const handleAccessToken = React.useCallback(({ token, redirectArgs: args }: CallbackArgs) => {
    setConfig({
      mode: args.mode,
      signup: args.signup,
      token: token?.accessToken,
      loading: true,
    });

    if (!token?.accessToken) {
      setConfig({
        mode: args.mode,
        signup: args.signup,
        token: token?.accessToken,
        loading: false,
      });
      return;
    }

    doAuth(
      args.mode,
      args.signup,
      args.redirect_uri,
      token?.accessToken,
      token?.idToken,
      args.invite_token,
    ).then();
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
    if (config?.mode === 'cli_token') {
      return (
        <CLICodeBox
          code={config.token}
        />
      );
    }

    return loadingMessage;
  }, [config?.mode, config?.token, loadingMessage]);

  const loading = !config || config.loading;
  return (
    <BasePage>
      {loading && loadingMessage}
      {!loading && (config.err ? <ErrorMessage config={config} /> : normalMessage)}
    </BasePage>
  );
});
AuthCallbackPage.displayName = 'AuthCallbackPage';

export const CLITokenPage: React.FC = React.memo(() => {
  const { token, mode, redirect } = QueryString.parse(window.location.search);
  const tokenStr = typeof token === 'string' ? token : '';
  const [loading, setLoading] = React.useState(true);

  React.useEffect(() => {
    if (mode === 'cli_get' && typeof redirect === 'string') {
      redirectGet(redirect, { accessToken: tokenStr }).then((response) => {
        if (response.status === 200 && response.data === 'OK') {
          RedirectUtils.redirect('/auth/cli-auth-complete', {});
        }
      }).catch(() => {
        // passthrough
        setLoading(false);
      });
    } else {
      setLoading(false);
    }
  }, [mode, redirect, tokenStr, setLoading]);

  if (loading) {
    return <></>;
  }

  return (
    <CLICodeBox
      code={tokenStr}
    />
  );
});
CLITokenPage.displayName = 'CLITokenPage';
