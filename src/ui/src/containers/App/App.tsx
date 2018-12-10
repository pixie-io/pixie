import {Auth} from 'containers/Auth';
import {Home} from 'containers/Home';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import * as fetch from 'unfetch';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import {createHttpLink} from 'apollo-link-http';
import { ApolloProvider } from 'react-apollo';

export interface AppProps {
  name: string;
}

export const link = createHttpLink({
  uri: '/gql',
  fetch,
});
const gqlCache = new InMemoryCache();
const gqlClient = new ApolloClient({cache: gqlCache, link});

export class App extends React.Component<AppProps, any> {
  render() {
    return (
      <Router>
        <ApolloProvider client={gqlClient}>
          <div>
            Pixie Labs UI!
            <Route exact path='/' component={Home} />
            <Route path='/login' component={Auth} />
          </div>
        </ApolloProvider>
      </Router>
    );
  }
}

export default App;
