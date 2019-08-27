import Axios from 'axios';
import {fetch as fetchPolyfill} from 'whatwg-fetch';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import gql from 'graphql-tag';
import { ApolloProvider } from 'react-apollo';

export const GET_CLUSTER_CONN = gql`
{
  clusterConnection {
    ipAddress
    token
  }
}`;

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

const vizierFetch = (uri, options) => {
  // Attempt to execute an initial fetch.
  const auth: string = localStorage.getItem('vizierAuth');
  let token: string = '';
  let address: string = '';
  if (auth != null) {
    const parsedAuth = JSON.parse(auth);
    address = parsedAuth.address;
    token = parsedAuth.token;
  }
  options.headers.authorization = `Bearer ${token}`;

  const initialRequest = fetchPolyfill(address + uri, options);

  this.refreshingToken = null;

  return initialRequest.then((response) => {
    return response.text();
  }).then((respText) => {
    // If no errors, repackage response into expected return format.
    const result = {} as FetchResponse;
    result.ok = true;
    result.text = () => new Promise((resolve, reject) => {
      resolve(respText);
    });
    return result;
  }).catch((err) => {
    if (!this.refreshingToken) {
      this.refreshingToken = cloudClient.query({
        query: GET_CLUSTER_CONN,
      });
    }
    return this.refreshingToken.then(({loading, error, data}) => {
      this.refreshingToken = null;

      const newToken = data.clusterConnection.token;
      const newAddress = 'https://' + data.clusterConnection.ipAddress;
      localStorage.setItem('vizierAuth', JSON.stringify({
        token: newToken,
        address: newAddress,
      }));

      options.headers.authorization = `Bearer ${newToken}`;
      return fetchPolyfill(newAddress + uri, options);
    }).catch((error) => {
      this.refreshingToken = null;
      const errResult = {} as FetchResponse;
      errResult.ok = false;
      return errResult;
    });
  });
};

const vzLink = createHttpLink({
  uri: '/graphql',
  fetch: vizierFetch,
});

const gqlCache = new InMemoryCache();

export const gqlClient = new ApolloClient({cache: gqlCache, link:
  ApolloLink.split((operation) => operation.getContext().clientName !== 'vizier',
    cloudAuthLink.concat(cloudLink),
    vzLink,
)});

const cloudClient = new ApolloClient({cache: gqlCache, link: cloudAuthLink.concat(cloudLink)});
