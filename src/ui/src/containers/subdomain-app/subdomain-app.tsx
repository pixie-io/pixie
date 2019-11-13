import './subdomain-app.scss';

import {InMemoryCache} from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import {ApolloLink} from 'apollo-link';
import {setContext} from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import {Home} from 'containers/Home';
import {Login} from 'containers/login';
import {Vizier} from 'containers/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {BrowserRouter as Router, Link, Route, Switch} from 'react-router-dom';
import {fetch} from 'unfetch/polyfill';

import {gqlClient} from './gql-client';

export interface SubdomainAppProps {
  name: string;
}

export class SubdomainApp extends React.Component<SubdomainAppProps, {}> {
  render() {
    return (
      <Router>
        <ApolloProvider client={gqlClient}>
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
