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

import {
  DARK_THEME, LIGHT_THEME, SnackbarProvider, VersionInfo,
} from 'app/components';
import Live from 'app/containers/App/live';
import PixieCookieBanner from 'configurable/cookie-banner';
import { LD_CLIENT_ID } from 'app/containers/constants';
import {
  Redirect, Route, Router, Switch,
} from 'react-router-dom';
import { makeCancellable, silentlyCatchCancellation } from 'app/utils/cancellable-promise';
import { isProd, PIXIE_CLOUD_VERSION } from 'app/utils/env';
import history from 'app/utils/pl-history';
import * as QueryString from 'query-string';

import {
  ThemeProvider, withStyles,
} from '@material-ui/core/styles';
import StyledEngineProvider from '@material-ui/core/StyledEngineProvider';
import { createStyles } from '@material-ui/styles';

import Axios from 'axios';
import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { CssBaseline } from '@material-ui/core';
import { withLDProvider } from 'launchdarkly-react-client-sdk';
import { AuthRouter } from 'app/pages/auth/auth';
import 'typeface-roboto';
import 'typeface-roboto-mono';
import { PixieAPIContext, PixieAPIContextProvider } from 'app/api';
import { AuthContextProvider, AuthContext } from 'app/common/auth-context';
import { EmbedContext, EmbedContextProvider } from 'app/common/embed-context';

// This side-effect-only import has to be a `require`, or else it gets erroneously optimized away during compilation.
require('./wdyr');

const RedirectWithArgs = (props) => {
  const {
    from,
    to,
    exact,
    location,
  } = props;

  return (
    <Redirect
      from={from}
      exact={exact}
      to={{ pathname: to, search: location.search }}
    />
  );
};

function useIsAuthenticated() {
  // Using an object instead of separate variables because using multiple setState does NOT batch if it happens outside
  // of React's scope (like resolved promises or Observable subscriptions). To make it atomic, have to use ONE setState.
  const [{ loading, authenticated, error }, setState] = React.useState({
    loading: true, authenticated: false, error: undefined,
  });

  const client = React.useContext(PixieAPIContext);
  React.useEffect(() => {
    if (!client) throw new Error('useIsAuthenticated needs to be called within a PixieAPIContextProvider!');

    setState({ loading: true, authenticated, error: undefined });
    const authPromise = makeCancellable(client.isAuthenticated());
    authPromise
      .then((isAuthenticated) => {
        setState({ loading: false, authenticated: isAuthenticated, error: undefined });
      })
      .catch(silentlyCatchCancellation)
      .catch((e) => {
        setState({ loading: false, authenticated: false, error: e });
      });

    return () => authPromise.cancel();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [client]);

  return { authenticated, loading, error };
}

export const App: React.FC = () => {
  const { authenticated, loading } = useIsAuthenticated();
  const { authToken } = React.useContext(AuthContext);

  const isEmbedded = window.location.pathname.startsWith('/embed');

  // If in an embedded environment, we need to wait until the authToken has been sent over from the parent.
  // While there is no authToken, we should not render the page, as all GQL requests will fail.
  if (isEmbedded && authToken.length === 0) {
    return null;
  }

  const authRedirectUri = window.location.pathname.length > 1
    ? encodeURIComponent(window.location.pathname + window.location.search)
    : '';
  const authRedirectTo = authRedirectUri ? `/login?redirect_uri=${authRedirectUri}` : '/login';

  return loading ? null : (
    <>
      <SnackbarProvider>
        <Router history={history}>
          <div className='center-content'>
            <Switch>
              <Route path='/auth' component={AuthRouter} />
              <RedirectWithArgs exact from='/login' to='/auth/login' />
              <RedirectWithArgs exact from='/logout' to='/auth/logout' />
              <RedirectWithArgs exact from='/signup' to='/auth/signup' />
              <RedirectWithArgs exact from='/auth-complete' to='/auth/cli-auth-complete' />
              {
                // 404s are handled within the Live route, after the user authenticates.
                // Logged out users get redirected to /login before the possibility of a 404 is checked.
                authenticated || isEmbedded ? <Route component={Live} />
                  : <Redirect from='/*' to={authRedirectTo} />
              }
            </Switch>
          </div>
        </Router>
        {!isProd() ? <VersionInfo cloudVersion={PIXIE_CLOUD_VERSION} /> : null}
      </SnackbarProvider>
      <PixieCookieBanner />
    </>
  );
};

// TODO(zasgar): Cleanup these styles. We probably don't need them all after we
// fix all of material styling.
const styles = () => createStyles({
  '@global': {
    '#root': {
      height: '100%',
      width: '100%',
    },
    html: {
      height: '100%',
    },
    body: {
      height: '100%',
      overflow: 'hidden',
      margin: 0,
      boxSizing: 'border-box',
    },
    ':focus': {
      outline: 'none !important',
    },
    // TODO(zasgar): remove this center content global.
    '.center-content': {
      height: '100%',
      width: '100%',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
    },
  },
});

let StyledApp = withStyles(styles)(App);

if (LD_CLIENT_ID !== '') {
  StyledApp = withLDProvider({
    clientSideID: LD_CLIENT_ID,
  })(StyledApp);
}

const ThemedApp: React.FC = () => {
  const { setAuthToken } = React.useContext(AuthContext);
  const { setTimeArg } = React.useContext(EmbedContext);
  const [theme, setTheme] = React.useState<string | string[]>('dark');
  const [embedToken, setEmbedToken] = React.useState<string>('');

  // Parse query params to determine initial state of the page. These
  // params can also be set by the parent view, in an embedded context.
  React.useEffect(() => {
    const {
      theme: themeParam,
    } = QueryString.parse(window.location.search);

    if (themeParam) {
      setTheme(Array.isArray(themeParam) ? themeParam[0] : themeParam);
    }
  }, [setTheme]);

  // This is for an embedded environment.
  const listener = React.useCallback(async (event) => {
    const {
      data:
        {
          // embedPixieAPIKey is the auth token, and embedPixieAPIToken is a pixie API key.
          parentReady,
          embedPixieAPIKey,
          pixieTheme,
          pixieStartTime,
          embedPixieAPIToken,
        },
    } = event;

    if (pixieStartTime) {
      setTimeArg(pixieStartTime);
    }

    if (pixieTheme && (pixieTheme === 'light' || pixieTheme === 'dark')) {
      setTheme(pixieTheme);
    }

    // If the parent sends a ready message, it probably missed Pixie's
    // initial ready message. Send out another ready message, which
    // we know it will receive.
    if (parentReady) {
      window.top.postMessage({ pixieEmbedReady: true }, '*');
    }

    // In the embedded environment, we
    // expect authentication to be done using an auth token. The auth token
    // will be POSTed over, and is used to get a Pixie access token that is
    // attached to our requests using bearer auth.
    if (embedPixieAPIKey || embedPixieAPIToken) {
      // Only request a new access token if sent a new token.
      if (embedPixieAPIKey) {
        if (embedToken === embedPixieAPIKey) {
          return;
        }
        setEmbedToken(embedToken);
      }

      let response = null;
      try {
        response = await Axios.post('/api/auth/loginEmbedNew', {
          accessToken: embedPixieAPIKey ?? '',
          orgName: '',
        }, embedPixieAPIToken ? { headers: { 'pixie-api-key': embedPixieAPIToken } } : null);
      } catch (err) {
        return;
      }

      setAuthToken(response.data.token);
    }
  }, [embedToken, setEmbedToken, setAuthToken, setTimeArg]);

  React.useEffect(() => {
    window.addEventListener('message', listener);
    // Send a message to the parent frame to inform it that Pixie is listening
    // for postMessages. We use top, to send it to the topmost window.
    // window.postMessage is not enough for a child to contact the parent.
    window.top.postMessage({ pixieEmbedReady: true }, '*');
    return () => {
      window.removeEventListener('beforeunload', listener);
    };
  }, [listener]);

  let themeStyles = DARK_THEME;
  switch (theme) {
    case 'light':
      themeStyles = LIGHT_THEME;
      break;
    default:
  }
  return (
    <ThemeProvider theme={themeStyles}>
      <CssBaseline />
        <PixieAPIContextProvider apiKey=''>
          <StyledApp />
        </PixieAPIContextProvider>
    </ThemeProvider>
  );
};

ReactDOM.render(
  <StyledEngineProvider injectFirst>
    <AuthContextProvider>
      <EmbedContextProvider>
        < ThemedApp />
      </EmbedContextProvider>
    </AuthContextProvider>
  </StyledEngineProvider>, document.getElementById('root'));
