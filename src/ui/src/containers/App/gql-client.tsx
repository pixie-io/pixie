import Axios from 'axios';
import {fetch as fetchPolyfill} from 'whatwg-fetch';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import gql from 'graphql-tag';
import { ApolloProvider } from 'react-apollo';

interface FetchResponse {
  ok: boolean;
  text: any;
}

const cloudLink = createHttpLink({
  uri: '/api/graphql',
  fetch: fetchPolyfill,
});

const cloudAuthLink = setContext((_, { headers }) => {
  return {
    headers: {
      ...headers,
      withCredentials: true,
    },
  };
});

const gqlCache = new InMemoryCache();

export const gqlClient = new ApolloClient({cache: gqlCache, link: cloudAuthLink.concat(cloudLink)});
