import './App.scss';

import Axios from 'axios';
import {getCloudGQLClient} from 'common/cloud-gql-client';
import {DARK_THEME} from 'common/mui-theme';
import {VersionInfo} from 'components/version-info/version-info';
import {AuthComplete} from 'containers/login/auth-complete';
import {Login} from 'containers/login/login';
import {Vizier} from 'containers/vizier/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Redirect, Route, Router, Switch} from 'react-router-dom';
import {isProd} from 'utils/env';
import history from 'utils/pl-history';

import {ThemeProvider} from '@material-ui/core/styles';

export class App extends React.Component {
  state = {
    client: null,
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
    }).catch((error) => {
        this.setState({ authenticated: false, loaded: true });
    });

    getCloudGQLClient().then((client) => {
        this.setState({ client });
    });
  }

  render() {
    const { client, authenticated, loaded } = this.state;
    return !client || !loaded ?
      null :
      (
        <ThemeProvider theme={DARK_THEME}>
          <Router history={history}>
            <ApolloProvider client={client}>
              <div className='pixie-main-app center-content'>
                <Switch>
                  <Route exact path='/auth-complete' component={AuthComplete} />
                  <Route exact path='/login' component={Login} />
                  <Route exact path='/logout' component={Login} />
                  <Route exact path='/signup' component={Login} />
                    {
                      authenticated ? <Route component={Vizier} /> :
                        <Redirect from='/*' to='/signup' />
                    }
                  <Route component={Vizier} />
                </Switch>
              </div>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </ThemeProvider>
      );
  }
}
