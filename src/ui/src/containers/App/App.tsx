import {Home} from 'containers/Home';
import {Login} from 'containers/login';
import {Vizier} from 'containers/vizier';

import * as React from 'react';
import { BrowserRouter as Router, Link, Route} from 'react-router-dom';
import {fetch} from 'unfetch/polyfill';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import { ApolloProvider } from 'react-apollo';
import {gqlClient} from './gql-client';

import './App.scss';

export interface AppProps {
  name: string;
}



export class App extends React.Component<AppProps, {}> {
  render() {
    return (
      <Router>
        <ApolloProvider client={gqlClient}>
          <div className='main-page'>
            <div className='content'>
              <Route exact path='/' component={Login} />
              <Route exact path='/create' component={Login} />
              <Route path='/login' component={Login} />
              <Route path='/create-site' component={Login} />
              <Route path='/vizier' component={Vizier} />
            </div>
          </div>
        </ApolloProvider>
      </Router>
    );
  }
}

export default App;
