import * as React from 'react';
import auth0 from 'auth0-js';
import { AUTH0_CLIENT_ID, AUTH0_DOMAIN } from 'containers/constants';
import * as QueryString from 'querystring';
import Axios, { AxiosError } from 'axios';
import * as RedirectUtils from 'utils/redirect-utils';
import { isValidAnalytics } from 'utils/env';
import { MessageBox } from 'components/auth/message';
import { BasePage } from './base';
import { AuthCallbackMode } from './utils';

const redirectPost = (url, data) => {
  const form = document.createElement('form');
  document.body.appendChild(form);
  form.method = 'post';
  form.action = url;
  Object.entries(data).forEach(([name, val]: [string, string]) => {
    const input = document.createElement('input');
    input.type = 'hidden';
    input.name = name;
    input.value = val;
    form.appendChild(input);
  });
  form.submit();
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
  <MessageBox
    title='Pixie Auth Token'
    message='Please copy this code, switch to the CLI and paste it there:'
    code={code}
  />
);

/**
 * This is the main component to handle the callback from auth.
 *
 * This component gets the token from Auth0 and either sends it to the CLI or
 * makes a request to Pixie cloud to perform a signup/login.
 */
export const AuthCallbackPage = () => {
  const [config, setConfig] = React.useState<CallbackConfig>(null);

  React.useEffect(() => {
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
      if (err.code === '401' || err.code === '403') {
        setErr('auth', err.response.data);
      } else {
        setErr('internal', err.response.data);
      }
    };

    const performSignup = async (accessToken: string) => {
      try {
        const response = await Axios.post('/api/auth/signup', { accessToken });
        analytics.identify(response.data.userInfo.userID);
        analytics.track('User signed up');
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
        analytics.identify(response.data.userInfo.userID);
        if (isValidAnalytics()) {
          await Promise.race([
            new Promise((resolve, reject) => { // Wait for analytics to be sent out before redirecting.
              analytics.track('User logged up', (err) => {
                if (err) {
                  reject();
                }
                resolve();
              });
            }),
            // Wait a maximum of 6s before redirecting. If it takes this long, it probably means that
            // something in Segment failed to initialize/send.
            new Promise((resolve) => setTimeout(resolve, 6000)),
          ]);
        }
        return true;
      } catch (err) {
        analytics.track('User signup failed', { error: err.response.data });
        handleHTTPError(err as AxiosError);
        return false;
      }
    };

    const sendTokenToCLI = async (accessToken: string, redirectURI: string) => {
      // eslint-disable-next-line @typescript-eslint/camelcase
      redirectPost(redirectURI, { access_token: accessToken });
    };

    const wa = new auth0.WebAuth({
      domain: AUTH0_DOMAIN,
      clientID: AUTH0_CLIENT_ID,
    });
    wa.parseHash({ hash: window.location.hash }, (errStatus, authResult) => {
      if (errStatus) {
        setErr('internal', `${errStatus.error} - ${errStatus.errorDescription}`);
        return;
      }

      const params = QueryString.parse(window.location.search.substr(1));
      let mode: AuthCallbackMode;
      switch (params.mode) {
        case 'cli_post':
        case 'cli_token':
        case 'ui':
          ({ mode } = params);
          break;
        default:
          mode = 'ui';
      }

      const location = params.location && String(params.location);
      const signup = !!params.signup;
      // eslint-disable-next-line @typescript-eslint/camelcase
      const orgName = params.org_name && String(params.org_name);
      // eslint-disable-next-line @typescript-eslint/camelcase
      const redirectURI = params.redirect_uri && String(params.redirect_uri);

      setConfig({
        mode,
        signup,
        token: authResult.accessToken,
        loading: true,
      });

      const doAuth = async () => {
        const token = authResult.accessToken;
        let signupSuccess = false;
        let loginSuccess = false;

        if (signup) {
          // We always need to perform signup, even if the mode is CLI.
          signupSuccess = await performSignup(token);
        }
        // eslint-disable-next-line default-case
        switch (mode) {
          case 'cli_post':
            await sendTokenToCLI(token, redirectURI);
            return;
          case 'cli_token':
            // Nothing to do, it will just render.
            break;
          case 'ui':
            if (!signup) {
              loginSuccess = await performUILogin(token, orgName);
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

      doAuth();
    });
  }, []);

  const renderError = () => {
    let title: string;
    let errorDetails: string;
    let message: string;
    if (config.signup) {
      title = 'Failed to Signup user';
      if (config.err.errorType === 'internal') {
        message = 'Sorry we hit a snag failed to create an account. Please try again later.';
        errorDetails = config.err.errMessage;
      } else {
        message = config.err.errMessage;
      }
    } else {
      // Login.
      title = 'Failed to authenticate';
      if (config.err.errorType === 'internal') {
        message = 'Sorry we hit a snag failed to authenticate you. Please try again later.';
        errorDetails = config.err.errMessage;
      } else {
        message = config.err.errMessage;
      }
    }

    return (
      <MessageBox
        error
        title={title}
        message={message}
        errorDetails={errorDetails}
      />
    );
  };
  const renderLoadingMessage = () => (
    <MessageBox
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
      { loading && renderLoadingMessage() }
      { !loading && (config.err ? renderError() : renderMessage())}
    </BasePage>
  );
};
