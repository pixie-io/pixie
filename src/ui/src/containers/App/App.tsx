import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {Auth} from 'containers/Auth';
import {Home} from 'containers/Home';
import {Vizier} from 'containers/vizier';
import * as React from 'react';
import { BrowserRouter as Router, Link, Route} from 'react-router-dom';
import {fetch} from 'unfetch/polyfill';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import { ApolloProvider } from 'react-apollo';

// TODO(zasgar/michelle): we should figure out a good way to
// package assets.
// @ts-ignore : TS does not like image files.
import * as infoImage from 'images/icons/agent.svg';
// @ts-ignore : TS does not like image files.
import * as codeImage from 'images/icons/query.svg';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/logo.svg';

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
            <SidebarNav
              logo = {logoImage}
              items={[
                { link: '/vizier/query', selectedImg: codeImage, unselectedImg: codeImage },
                { link: '/vizier/agents', selectedImg: infoImage, unselectedImg: infoImage },
              ]}
            />
            <div className='content'>
              <Route exact path='/' component={Home} />
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
