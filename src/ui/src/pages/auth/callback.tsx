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
import * as QueryString from 'query-string';
import Axios, { AxiosError } from 'axios';
import * as RedirectUtils from 'utils/redirect-utils';
import { isValidAnalytics } from 'utils/env';
import { AuthMessageBox } from '@pixie-labs/components';
import { Link } from 'react-router-dom';
import {
  Button, ButtonProps, createStyles, makeStyles, Theme,
} from '@material-ui/core';
import { BasePage } from './base';
import { AuthCallbackMode, GetOAuthProvider } from './utils';
import { Token } from './oauth-provider';

const redirectGet = async (url: string, data: { accessToken: string }) => {
  // TODO(philkuz) (PC-883) remove the data from the query string.
  const fullURL = QueryString.stringifyUrl({ url, query: data });
  // Send token header to enable CORS check. Token is still allowed with Pixie CLI.
  return Axios.get(fullURL, { headers: { token: data.accessToken } });
};

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

const CLICodeBox = ({ code }) => (
  <AuthMessageBox
    title='Pixie Auth Token'
    message='Please copy this code, switch to the CLI and paste it there:'
    code={code}
  />
);

const useStyles = makeStyles((theme: Theme) => createStyles({
  ctaGutter: {
    marginTop: theme.spacing(3),
    paddingTop: theme.spacing(3),
    borderTop: `1px solid ${theme.palette.foreground.grey1}`,
    width: '80%',
  },
}));

const CtaButton = ({ children, ...props }: ButtonProps) => (
  <Button color='primary' variant='contained' {...props}>{children}</Button>
);

const trackAuthEvent = (event: string, id: string, email: string) => {
  if (isValidAnalytics()) {
    return Promise.race([
      new Promise((resolve, reject) => { // Wait for analytics to be sent out before redirecting.
        analytics.track(event, (err) => {
          if (err) {
            reject();
          }
          analytics.identify(id, { email }, {}, () => {
            resolve();
          });
        });
      }),
      // Wait a maximum of 4s before redirecting. If it takes this long, it probably means that
      // something in Segment failed to initialize/send.
      new Promise((resolve) => setTimeout(resolve, 4000)),
    ]);
  }

  return Promise.resolve();
};

/**
 * This is the main component to handle the callback from auth.
 *
 * This component gets the token from Auth0 and either sends it to the CLI or
 * makes a request to Pixie cloud to perform a signup/login.
 */
export const AuthCallbackPage: React.FC = () => {
  const [config, setConfig] = React.useState<CallbackConfig>(null);
  const classes = useStyles();

  const setErr = (errType: ErrorType, errMsg: string) => {
    setConfig((c) => ({
      ...c,
      err: {
        errorType: errType,
        errMessage: errMsg,
      },
      loading: false,
    }));
  };

  const handleHTTPError = (err: AxiosError) => {
    if (err.code === '401' || err.code === '403' || err.code === '404') {
      setErr('auth', err.response.data);
    } else {
      setErr('internal', err.response.data);
    }
  };

  const performSignup = async (accessToken: string) => {
    try {
      const response = await Axios.post('/api/auth/signup', { accessToken });
      await trackAuthEvent('User signed up', response.data.userInfo.userID, response.data.userInfo.email);
      return true;
    } catch (err) {
      analytics.track('User signup failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
  };

  const performUILogin = async (accessToken: string, orgName: string) => {
    try {
      const response = await Axios.post('/api/auth/login', {
        accessToken,
        orgName,
      });
      await trackAuthEvent('User logged in', response.data.userInfo.userID, response.data.userInfo.email);
      return true;
    } catch (err) {
      analytics.track('User login failed', { error: err.response.data });
      handleHTTPError(err as AxiosError);
      return false;
    }
  };

  const sendTokenToCLI = async (accessToken: string, redirectURI: string) => {
    try {
      const response = await redirectGet(redirectURI, { accessToken });
      return response.status === 200 && response.data === 'OK';
    } catch (error) {
      handleHTTPError(error as AxiosError);
      // If there's an error, we just return a failure.
      return false;
    }
  };

  const doAuth = async (
    mode: AuthCallbackMode,
    signup: boolean,
    redirectURI: string,
    location: string,
    orgName: string,
    accessToken: string,
  ) => {
    let signupSuccess = false;
    let loginSuccess = false;

    if (signup) {
      // We always need to perform signup, even if the mode is CLI.
      signupSuccess = await performSignup(accessToken);
    }
    // eslint-disable-next-line default-case
    switch (mode) {
      case 'cli_get':
        loginSuccess = await sendTokenToCLI(accessToken, redirectURI);
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
          loginSuccess = await performUILogin(accessToken, orgName);
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
  };

  const handleAccessToken = (accessToken: string) => {
    const params = QueryString.parse(window.location.search.substr(1));
    let mode: AuthCallbackMode;
    switch (params.mode) {
      case 'cli_get':
      case 'cli_token':
      case 'ui':
        ({ mode } = params);
        break;
      default:
        mode = 'ui';
    }

    const location = params.location && String(params.location);
    const signup = !!params.signup;
    const orgName = params.org_name && String(params.org_name);
    const redirectURI = params.redirect_uri && String(params.redirect_uri);

    setConfig({
      mode,
      signup,
      token: accessToken,
      loading: true,
    });

    doAuth(mode, signup, redirectURI, location, orgName, accessToken);
  };

  React.useEffect(() => {
    GetOAuthProvider().handleToken().then(handleAccessToken).catch((err) => {
      setErr('internal', `${err}`);
    });
  }, []);

  const renderError = () => {
    const title = config.signup ? 'Failed to Sign Up' : 'Failed to Log In';
    const errorDetails = config.err.errorType === 'internal' ? config.err.errMessage : undefined;

    let ctaMessage: string;
    let ctaDestination: string;
    let errorMessage: string;
    if (config.err.errorType === 'internal') {
      if (config.signup) {
        errorMessage = 'We hit a snag in creating an account. Please try again later.';
        ctaMessage = 'Back to Sign Up';
        ctaDestination = '/auth/signup';
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

    const cta = (
      <div className={classes.ctaGutter}>
        <Link to={ctaDestination} component={CtaButton}>
          {ctaMessage}
        </Link>
      </div>
    );

    return (
      <AuthMessageBox
        error='recoverable'
        title={title}
        message={errorMessage}
        errorDetails={errorDetails}
        cta={cta}
      />
    );
  };
  const renderLoadingMessage = () => (
    <AuthMessageBox
      title='Authenticating'
      message={
        (config && config.signup) ? 'Signing up ...' : 'Logging in...'
          || '...'
      }
    />
  );
  const renderMessage = () => {
    if (config.mode === 'cli_token') {
      return (
        <CLICodeBox
          code={config.token}
        />
      );
    }

    return renderLoadingMessage();
  };

  const loading = !config || config.loading;
  return (
    <BasePage>
      { loading && renderLoadingMessage()}
      { !loading && (config.err ? renderError() : renderMessage())}
    </BasePage>
  );
};
