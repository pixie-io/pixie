import './wdyr';

import Axios from 'axios';
import { CloudClient } from '@pixie/api';
import { DARK_THEME, SnackbarProvider, VersionInfo } from '@pixie/components';
import Vizier from 'containers/App/vizier';
import PixieCookieBanner from 'common/cookie-banner';
import { LD_CLIENT_ID } from 'containers/constants';
import {
  Redirect, Route, Router, Switch,
} from 'react-router-dom';
import { isProd, PIXIE_CLOUD_VERSION } from 'utils/env';
import history from 'utils/pl-history';

import { ApolloProvider } from '@apollo/client';
import {
  createStyles, ThemeProvider, withStyles,
} from '@material-ui/core/styles';

import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { CssBaseline } from '@material-ui/core';
import { withLDProvider } from 'launchdarkly-react-client-sdk';
import { redirect } from 'utils/redirect-utils';
import { CloudClientContext } from 'context/app-context';
import { AuthRouter } from 'pages/auth/auth';
import 'typeface-roboto';
import 'typeface-roboto-mono';

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

export class App extends React.Component {
  // eslint-disable-next-line react/state-in-constructor
  state = {
    cloudClient: new CloudClient({
      uri: `${window.location.origin}/api`,
      onUnauthorized: () => {
        // eslint-disable-next-line @typescript-eslint/camelcase
        redirect('/login', { no_cache: 'true' });
      },
    }),
    gqlClient: null,
    authenticated: false,
    loaded: false,
  };

  componentDidMount() {
    Axios({
      method: 'get',
      url: '/api/authorized',
    }).then((response) => {
      if (response.status === 200) {
        this.setState({ authenticated: true, loaded: true });
      }
    }).catch(() => {
      // TODO(malthus): Do something with this error.
      this.setState({ authenticated: false, loaded: true });
    });

    this.state.cloudClient.getGraphQLPersist().then((gqlClient) => {
      this.setState({ gqlClient });
    });
  }

  render() {
    const {
      gqlClient, authenticated, loaded, cloudClient,
    } = this.state;
    const authRedirectUri = (window.location.pathname.length > 1
      ? encodeURIComponent(window.location.pathname + window.location.search)
      : '');
    const authRedirectTo = authRedirectUri ? `/login?redirect_uri=${authRedirectUri}` : '/login';

    return !gqlClient || !loaded
      ? null
      : (
        <CloudClientContext.Provider value={cloudClient}>
          <SnackbarProvider>
            <Router history={history}>
              <ApolloProvider client={gqlClient}>
                <div className='center-content'>
                  <Switch>
                    <Route path='/auth' component={AuthRouter} />
                    <RedirectWithArgs exact from='/login' to='/auth/login' />
                    <RedirectWithArgs exact from='/logout' to='/auth/logout' />
                    <RedirectWithArgs exact from='/signup' to='/auth/signup' />
                    <RedirectWithArgs exact from='/auth-complete' to='/auth/cli-auth-complete' />
                    {
                      authenticated ? <Route component={Vizier} />
                        : <Redirect from='/*' to={authRedirectTo} />
                    }
                  </Switch>
                </div>
              </ApolloProvider>
            </Router>
            {!isProd() ? <VersionInfo cloudVersion={PIXIE_CLOUD_VERSION} /> : null}
          </SnackbarProvider>
          <PixieCookieBanner />
        </CloudClientContext.Provider>
      );
  }
}

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

const StyledApp = withLDProvider({
  clientSideID: LD_CLIENT_ID,
})(withStyles(styles)(App));

ReactDOM.render(
  <ThemeProvider theme={DARK_THEME}>
    <CssBaseline />
    <StyledApp />
  </ThemeProvider>, document.getElementById('root'));
