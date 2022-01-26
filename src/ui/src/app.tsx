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

import { CssBaseline } from '@mui/material';
import {
  Theme,
  ThemeProvider,
  StyledEngineProvider,
  createTheme,
} from '@mui/material/styles';
import Axios from 'axios';
import { withLDProvider } from 'launchdarkly-react-client-sdk';
import * as QueryString from 'query-string';
import * as ReactDOM from 'react-dom';
import { useLocation } from 'react-router';
import {
  Redirect, RedirectProps, Route, Router, Switch,
} from 'react-router-dom';

import { PixieAPIContext, PixieAPIContextProvider } from 'app/api';
import { AuthContextProvider, AuthContext } from 'app/common/auth-context';
import { EmbedContext, EmbedContextProvider } from 'app/common/embed-context';
import {
  COMMON_THEME, DARK_THEME, LIGHT_THEME, SnackbarProvider, VersionInfo,
} from 'app/components';
import Live from 'app/containers/App/live';
import { LD_CLIENT_ID } from 'app/containers/constants';
import { AuthRouter } from 'app/pages/auth/auth';
import CreditsView from 'app/pages/credits/credits';
import { makeCancellable, silentlyCatchCancellation } from 'app/utils/cancellable-promise';
import { isProd, PIXIE_CLOUD_VERSION } from 'app/utils/env';
import { ErrorBoundary, PixienautCrashFallback } from 'app/utils/error-boundary';
import { parseJWT } from 'app/utils/jwt';
import history from 'app/utils/pl-history';
import { dateToEpoch } from 'app/utils/time';
import { PixieCookieBanner } from 'configurable/cookie-banner';

import 'typeface-roboto';
import 'typeface-roboto-mono';

// This side-effect-only import has to be a `require`, or else it gets erroneously optimized away during compilation.
require('./wdyr');

// If in embedded mode, the amount of time before the auth token expires in which a refresh token should
// be requested from the parent.
const REFRESH_TOKEN_TIMEOUT_S = 60 * 5; // 5 minutes

const RedirectWithArgs = React.memo<Pick<RedirectProps, 'from' | 'to' | 'exact'>>(
  ({ from, to, exact }) => {
    const location = useLocation();
    return (
      <Redirect
        from={from}
        exact={exact}
        to={React.useMemo(() => ({ pathname: to, search: location.search }), [to, location.search])}
      />
    );
  },
);
RedirectWithArgs.displayName = 'RedirectWithArgs';

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

function getAuthRedirectLocation(): string {
  let authRedirectUri: string;
  const path = window.location.pathname;
  if (path.length && path !== '/' && !path.endsWith('/login') && !path.endsWith('/logout')) {
    authRedirectUri = encodeURIComponent(path + window.location.search);
  }
  return authRedirectUri ? `/login?redirect_uri=${authRedirectUri}` : '/login';
}

// eslint-disable-next-line react-memo/require-memo
export const App: React.FC = () => {
  const { authenticated, loading } = useIsAuthenticated();
  const { authToken } = React.useContext(AuthContext);

  const isEmbedded = window.location.pathname.startsWith('/embed');

  // If in an embedded environment, we need to wait until the authToken has been sent over from the parent.
  // While there is no authToken, we should not render the page, as all GQL requests will fail.
  if (isEmbedded && authToken.length === 0) {
    return null;
  }

  const authRedirectTo = getAuthRedirectLocation();

  return loading ? null : (
    <ErrorBoundary name='App' fallback={PixienautCrashFallback}>
      <SnackbarProvider>
        <Router history={history}>
          <Switch>
            <Route path='/credits' component={CreditsView} />
            <Route path='/auth' component={AuthRouter} />
            <RedirectWithArgs exact from='/login' to='/auth/login' />
            <RedirectWithArgs exact from='/logout' to='/auth/logout' />
            <RedirectWithArgs exact from='/signup' to='/auth/signup' />
            <RedirectWithArgs exact from='/invite' to='/auth/invite' />
            <RedirectWithArgs exact from='/auth-complete' to='/auth/cli-auth-complete' />
            {
              // 404s are handled within the Live route, after the user authenticates.
              // Logged out users get redirected to /login before the possibility of a 404 is checked.
              authenticated || isEmbedded ? <Route component={Live} />
                : <Redirect from='/*' to={authRedirectTo} />
            }
          </Switch>
        </Router>
        {!isProd() ? <VersionInfo cloudVersion={PIXIE_CLOUD_VERSION} /> : null}
      </SnackbarProvider>
      <PixieCookieBanner />
    </ErrorBoundary>
  );
};
App.displayName = 'App';

const FlaggedApp = LD_CLIENT_ID !== ''
  ? withLDProvider({ clientSideID: LD_CLIENT_ID })(App)
  : App;

// eslint-disable-next-line react-memo/require-memo
const ThemedApp: React.FC = () => {
  const { setAuthToken } = React.useContext(AuthContext);
  const { setTimeArg } = React.useContext(EmbedContext);
  const [theme, setTheme] = React.useState<Theme>(DARK_THEME);
  const [embedToken, setEmbedToken] = React.useState<string>('');

  const setThemeFromName = React.useCallback((themeName) => {
    switch (themeName) {
      case 'light':
        setTheme(LIGHT_THEME);
        break;
      default:
        setTheme(DARK_THEME);
    }
  }, [setTheme]);

  const parseAndSetTheme = React.useCallback((customTheme: string) => {
    // Try to parse theme and apply.
    try {
      const parsedTheme = JSON.parse(customTheme);
      // Only use the `palette` field from the theme, as we know these
      // values are safe to apply.
      setTheme(createTheme({
        ...COMMON_THEME,
        ...{
          palette: {
            ...COMMON_THEME.palette,
            ...parsedTheme.palette,
          },
          shadows: {
            ...COMMON_THEME,
            ...parsedTheme.shadows,
          },
        },
      }));
    } catch (e) {
      // eslint-disable-next-line no-console
      console.error('Failed to parse MUI theme');
    }
  }, [setTheme]);

  // Parse query params to determine initial state of the page. These
  // params can also be set by the parent view, in an embedded context.
  React.useEffect(() => {
    const {
      theme: themeParam,
      customTheme,
    } = QueryString.parse(window.location.search);

    if (themeParam) {
      const themeName = Array.isArray(themeParam) ? themeParam[0] : themeParam;
      setThemeFromName(themeName);
    }
    if (customTheme) {
      parseAndSetTheme(Array.isArray(customTheme) ? customTheme[0] : customTheme);
    }
  }, [parseAndSetTheme, setThemeFromName]);

  // This is for an embedded environment.
  const listener = React.useCallback(async (event) => {
    const {
      data:
        {
          // embedPixieAPIKey is the auth token, and embedPixieAPIToken is a pixie API key.
          parentReady,
          embedPixieAPIKey,
          embedPixieToken,
          pixieTheme,
          pixieStartTime,
          embedPixieAPIToken,
          pixieStyles,
        },
    } = event;

    if (pixieStartTime) {
      setTimeArg(pixieStartTime);
    }

    if (pixieStyles) {
      // Try to parse theme and apply.
      parseAndSetTheme(pixieStyles);
    }

    if (pixieTheme && (pixieTheme === 'light' || pixieTheme === 'dark')) {
      setThemeFromName(pixieTheme);
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
    if (embedPixieAPIKey || embedPixieAPIToken || embedPixieToken) {
      if (embedPixieToken) {
        // This may mean that the parent view's token has expired. Send out a postMessage
        // asking for this token to be refreshed.
        setAuthToken(embedPixieToken);

        // Find the expiry time of the JWT and make sure we send a request to the parent for the refreshToken
        // before it expires.
        const jwt = parseJWT(embedPixieToken);
        if (jwt != null) {
          const refreshTimeout = jwt.exp - dateToEpoch(new Date(Date.now())) - REFRESH_TOKEN_TIMEOUT_S;
          setTimeout(() => {
            window.top.postMessage({ pixieRefreshToken: true }, '*');
          }, refreshTimeout * 1000);
        }
        return;
      }

      // Only request a new access token if sent a new token.
      if (embedPixieAPIKey) {
        if (embedToken === embedPixieAPIKey) {
          return;
        }
        setEmbedToken(embedToken);
      }

      let response = null;
      try {
        response = await Axios.post('/api/auth/loginEmbed', {
          accessToken: embedPixieAPIKey ?? '',
          orgName: '',
        }, embedPixieAPIToken ? { headers: { 'pixie-api-key': embedPixieAPIToken } } : null);
      } catch (err) {
        return;
      }

      setAuthToken(response.data.token);
    }
  }, [embedToken, setEmbedToken, setAuthToken, setTimeArg, parseAndSetTheme, setThemeFromName]);

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

  const onUnauthorized = React.useCallback(() => {
    const isEmbedded = window.location.pathname.startsWith('/embed');
    const isLogin = window.location.pathname.endsWith('/login');
    if (!isEmbedded && !isLogin) {
      const path = window.location.origin + getAuthRedirectLocation();
      if (path !== window.location.href) {
        window.location.href = path;
      }
    }
  }, []);

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <PixieAPIContextProvider apiKey='' onUnauthorized={onUnauthorized}>
        <FlaggedApp />
      </PixieAPIContextProvider>
    </ThemeProvider>
  );
};
ThemedApp.displayName = 'ThemedApp';

ReactDOM.render(
  <ErrorBoundary name='Root'>
    <StyledEngineProvider injectFirst>
      <AuthContextProvider>
        <EmbedContextProvider>
          <ThemedApp />
        </EmbedContextProvider>
      </AuthContextProvider>
    </StyledEngineProvider>
  </ErrorBoundary>, document.getElementById('root'));
