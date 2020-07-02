import Axios from 'axios';
import { CloudClient } from 'common/cloud-gql-client';
import { DARK_THEME } from 'common/mui-theme';
import { SnackbarProvider } from 'components/snackbar/snackbar';
import VersionInfo from 'components/version-info/version-info';
import { AuthComplete } from 'pages/login/auth-complete';
import Login from 'pages/login';
import Vizier from 'containers/App/vizier';
import {
  Redirect, Route, Router, Switch,
} from 'react-router-dom';
import { isProd } from 'utils/env';
import history from 'utils/pl-history';

import { ApolloProvider } from '@apollo/react-hooks';
import {
  createStyles, ThemeProvider, withStyles,
} from '@material-ui/core/styles';

import * as React from 'react';
import * as ReactDOM from 'react-dom';
import { CssBaseline } from '@material-ui/core';
import { CloudClientContext } from './context/app-context';

export class App extends React.Component {
  // eslint-disable-next-line react/state-in-constructor
  state = {
    cloudClient: new CloudClient(),
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
    }).catch((/* error */) => {
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
    return !gqlClient || !loaded
      ? null
      : (
        <CloudClientContext.Provider value={cloudClient}>
          <SnackbarProvider>
            <Router history={history}>
              <ApolloProvider client={gqlClient}>
                <div className='center-content'>
                  <Switch>
                    <Route exact path='/auth-complete' component={AuthComplete} />
                    <Route exact path='/login' component={Login} />
                    <Route exact path='/logout' component={Login} />
                    <Route exact path='/signup' component={Login} />
                    {
                      authenticated ? <Route component={Vizier} />
                        : <Redirect from='/*' to='/login' />
                    }
                  </Switch>
                </div>
              </ApolloProvider>
            </Router>
            {!isProd() ? <VersionInfo /> : null}
          </SnackbarProvider>
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

const StyledApp = withStyles(styles)(App);

ReactDOM.render(
  <ThemeProvider theme={DARK_THEME}>
    <CssBaseline />
    <StyledApp />
  </ThemeProvider>, document.getElementById('root'));
