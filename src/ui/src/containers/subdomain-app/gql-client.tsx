import Axios from 'axios';
import {fetch as fetchPolyfill} from 'whatwg-fetch';

import { InMemoryCache } from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import {createHttpLink} from 'apollo-link-http';
import {DOMAIN_NAME} from 'containers/constants';
import gql from 'graphql-tag';
import { ApolloProvider } from 'react-apollo';
import * as RedirectUtils from 'utils/redirect-utils';

const TIMEOUT_MS = 5000; // Timeout after 5 seconds.

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

const cloudFetch = (uri, options) => {
  options.headers.withCredentials = true;
  return fetchPolyfill(uri, options).then((resp) => {
    if (resp.status === 401) { // Unauthorized. Cookies are not valid, redirect to login.
      const subdomain = window.location.host.split('.')[0];
      RedirectUtils.redirect('id', '/login', {['site_name']: subdomain});
    }
    const result = {} as FetchResponse;
    result.ok = true;
    result.text = () => new Promise((resolve, reject) => {
      resolve(resp.text());
    });
    return result;
  });
};

const cloudLink = createHttpLink({
  uri: '/api/graphql',
  fetch: cloudFetch,
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
  let mode: string = '';
  let address: string = 'http://vizier-cluster';
  if (auth != null) {
    const parsedAuth = JSON.parse(auth);
    address = parsedAuth.address;
    token = parsedAuth.token;
    mode = parsedAuth.mode;
  }
  options.headers.authorization = `Bearer ${token}`;

  // Add timeout, in case the initial request hangs.
  const timeout = new Promise((resolve, reject) => {
    const id = setTimeout(() => {
      clearTimeout(id);
      resolve(null);
    }, TIMEOUT_MS);
  });

  const localhostRequest = mode === 'vizier' ?
    Promise.reject('Should use Vizier mode') : fetchPolyfill('http://127.0.0.1:31067' + uri, options);

  this.refreshingToken = null;

  return localhostRequest.then((response) => {
    if (!response || response.status !== 200) {
      throw new Error('failed localhost request');
    }

    return response.text();
  }).then((respText) => {
    // If no errors, repackage response into expected return format.
    const result = {} as FetchResponse;
    result.ok = true;
    result.text = () => new Promise((resolve, reject) => {
      resolve(respText);
    });
    return result;
  }).catch((localhostError) => {
    const initialRequest = fetchPolyfill(address + uri, options);
    return Promise.race([initialRequest, timeout]).then((response) => {
      if (!response || response.status !== 200) {
        throw new Error('failed initial request');
      }

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
        const newAddress = data.clusterConnection.ipAddress;
        localStorage.setItem('vizierAuth', JSON.stringify({
          token: newToken,
          address: newAddress,
          mode: 'vizier',
        }));

        options.headers.authorization = `Bearer ${newToken}`;
        return fetchPolyfill(newAddress + uri, options);
      }).catch((error) => {
        const lsAuth: string = localStorage.getItem('vizierAuth');
        if (lsAuth != null) {
          const parsedLsAuth = JSON.parse(lsAuth);
          // Vizier DNS failed, so reset mode to enable search for localhost again.
          localStorage.setItem('vizierAuth', JSON.stringify({
            token: parsedLsAuth.token,
            address: parsedLsAuth.address,
            mode: '',
          }));
        }

        this.refreshingToken = null;
        const errResult = {} as FetchResponse;
        errResult.ok = false;
        return errResult;
      });
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
