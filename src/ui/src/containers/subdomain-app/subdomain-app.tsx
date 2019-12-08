import './subdomain-app.scss';

import {cloudGQLClient} from 'common/cloud-gql-client';
import {VersionInfo} from 'components/version-info/version-info';
import {Login} from 'containers/login';
import {Vizier} from 'containers/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Route, Router, Switch, withRouter} from 'react-router-dom';
import {isProd, PIXIE_CLOUD_VERSION} from 'utils/env';
import history from 'utils/pl-history';

export interface SubdomainAppProps {
  name: string;
}

export class SubdomainApp extends React.Component<SubdomainAppProps, {}> {
  render() {
    return (
      <>
        <Router history={history}>
          <ApolloProvider client={cloudGQLClient}>
            <div style={{
              height: '100vh',
              width: '100vw',
              display: 'flex',
              flexDirection: 'column',
              overflow: 'auto',
            }}>
              <Switch>
                <Route path='/login' component={Login} />
                <Route path='/create-site' component={Login} />
                <Route path='/logout' component={Login} />
                <Route component={Vizier} />
              </Switch>
            </div>
          </ApolloProvider>
        </Router>
        {!isProd() ? <VersionInfo /> : null}
      </>
    );
  }
}

export default withRouter(SubdomainApp);
