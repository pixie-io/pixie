import {InMemoryCache} from 'apollo-cache-inmemory';
import {persistCache} from 'apollo-cache-persist';
import {ApolloClient} from 'apollo-client';
import {ApolloLink} from 'apollo-link';
import {setContext} from 'apollo-link-context';
import {onError} from 'apollo-link-error';
import {createHttpLink} from 'apollo-link-http';
import {ServerError} from 'apollo-link-http-common';
import gql from 'graphql-tag';
import * as RedirectUtils from 'utils/redirect-utils';
import {fetch} from 'whatwg-fetch';

import {localGQLResolvers, localGQLTypeDef} from './local-gql';

// Apollo link that adds cookies in the request.
const cloudAuthLink = setContext((_, { headers }) => {
  return {
    headers: {
      ...headers,
      withCredentials: true,
    },
  };
});

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = onError(({ networkError }) => {
  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    const subdomain = window.location.host.split('.')[0];
    RedirectUtils.redirect('id', '/login', { ['site_name']: subdomain, ['no_cache']: 'true' });
  }
});

const cache = new InMemoryCache();
const client = new ApolloClient({
  cache,
  link: ApolloLink.from([
    cloudAuthLink,
    loginRedirectLink,
    createHttpLink({ uri: '/api/graphql', fetch }),
  ]),
  resolvers: localGQLResolvers,
  typeDefs: localGQLTypeDef,
});

let loaded = false;

// getCloudGQLClientSync returns the GQL client immediately,
// without waiting for cache persist initialization.
export function getCloudGQLClientSync() {
  return client;
}

export async function getCloudGQLClient() {
  if (loaded) {
    return client;
  }

  await persistCache({
    cache,
    storage: window.localStorage,
  });
  loaded = true;

  return client;
}

interface GetClusterConnResults {
  clusterConnection: {
    ipAddress: string;
    token: string;
  };
}

const GET_CLUSTER_CONN = gql`
{
  clusterConnection {
    ipAddress
    token
  }
}`;

export async function getClusterConnection(noCache: boolean = false) {
  const gqlClient = await getCloudGQLClient();
  const { data } = await gqlClient.query<GetClusterConnResults>({
    query: GET_CLUSTER_CONN,
    fetchPolicy: noCache ? 'network-only' : 'cache-first',
  });
  return data.clusterConnection;
}
