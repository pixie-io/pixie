import {Auth} from 'containers/Auth';
import {Home} from 'containers/Home';
import {Login} from 'containers/login';
import {Vizier} from 'containers/vizier';

import * as React from 'react';
import { BrowserRouter as Router, Link, Route} from 'react-router-dom';
import {fetch} from 'unfetch/polyfill';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import { ApolloProvider } from 'react-apollo';

import './App.scss';

export interface AppProps {
  name: string;
}

export const link = createHttpLink({
  uri: '/graphql',
  fetch,
});

// Pull auth from the local storage.
const authLink = setContext((_, { headers }) => {
  // get the authentication token from local storage if it exists
  const auth: string = localStorage.getItem('auth');
  let token: string = '';
  if (auth != null) {
    token = JSON.parse(auth).idToken;
  }
  // TOOD(zasgar/michelle): If auth is not valid or present, we should redirect
  // to the login page.

  // Return the headers to the context so httpLink can read them.
  return {
    headers: {
      ...headers,
      authorization: `Bearer ${token}`,
    },
  };
});

const gqlCache = new InMemoryCache();
const gqlClient = new ApolloClient({cache: gqlCache, link: authLink.concat(link)});

export class App extends React.Component<AppProps, {}> {
  render() {
    return (
      <Router>
        <ApolloProvider client={gqlClient}>
          <div className='main-page'>
            <div className='content'>
              <Route exact path='/' component={Login} />
              <Route exact path='/create' component={Login} />
              <Route path='/login' component={Auth} />
              <Route path='/vizier' component={Vizier} />
            </div>
          </div>
        </ApolloProvider>
      </Router>
    );
  }
}

export default App;
