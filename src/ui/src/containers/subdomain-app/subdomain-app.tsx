import './subdomain-app.scss';

import {cloudGQLClient} from 'common/cloud-gql-client';
import {Login} from 'containers/login';
import {Vizier} from 'containers/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {BrowserRouter as Router, Link, Route, Switch} from 'react-router-dom';

export interface SubdomainAppProps {
  name: string;
}

export class SubdomainApp extends React.Component<SubdomainAppProps, {}> {
  render() {
    return (
      <Router>
        <ApolloProvider client={cloudGQLClient}>
          <div style={{ height: '100vh', width: '100vw', display: 'flex', flexDirection: 'column', overflow: 'auto' }}>
            <Switch>
              <Route path='/login' component={Login} />
              <Route path='/create-site' component={Login} />
              <Route path='/logout' component={Login} />
              <Route component={Vizier} />
            </Switch>
          </div>
        </ApolloProvider>
      </Router>
    );
  }
}

export default SubdomainApp;
