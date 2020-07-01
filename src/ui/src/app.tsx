import './index.scss';

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
import { ThemeProvider } from '@material-ui/core/styles';

import * as React from 'react';
import * as ReactDOM from 'react-dom';
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
          <ThemeProvider theme={DARK_THEME}>
            <SnackbarProvider>
              <Router history={history}>
                <ApolloProvider client={gqlClient}>
                  <div className='pixie-main-app center-content'>
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
          </ThemeProvider>
        </CloudClientContext.Provider>
      );
  }
}

ReactDOM.render(<App />, document.getElementById('root'));
