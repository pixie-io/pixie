import { InMemoryCache, NormalizedCacheObject } from 'apollo-cache-inmemory';
import { persistCache } from 'apollo-cache-persist';
import { ApolloClient } from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import { onError } from 'apollo-link-error';
import { createHttpLink } from 'apollo-link-http';
import { ServerError } from 'apollo-link-http-common';
import gql from 'graphql-tag';
import * as RedirectUtils from 'utils/redirect-utils';
import { fetch } from 'whatwg-fetch';

import { localGQLResolvers, localGQLTypeDef } from './local-gql';

// Apollo link that adds cookies in the request.
const cloudAuthLink = setContext((_, { headers }) => ({
  headers: {
    ...headers,
    withCredentials: true,
  },
}));

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = onError(({ networkError }) => {
  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    RedirectUtils.redirect('/login', { no_cache: 'true' });
  }
});

interface ClusterConnection {
  ipAddress: string;
  token: string;
}

interface GetClusterConnResults {
  clusterConnection: ClusterConnection;
}

const GET_CLUSTER_CONN = gql`
query GetClusterConnection($id: ID) {
  clusterConnection(id: $id) {
    ipAddress
    token
  }
}`;

export class CloudClient {
  graphQL: ApolloClient<NormalizedCacheObject>;

  private persistPromise: Promise<void>;

  private cache: InMemoryCache;

  private loaded = false;

  constructor() {
    this.cache = new InMemoryCache();
    this.graphQL = new ApolloClient({
      cache: this.cache,
      link: ApolloLink.from([
        cloudAuthLink,
        loginRedirectLink,
        createHttpLink({ uri: '/api/graphql', fetch }),
      ]),
      resolvers: localGQLResolvers,
      typeDefs: localGQLTypeDef,
    });

    this.persistPromise = persistCache({
      cache: this.cache,
      storage: window.localStorage,
    }).then(() => {
      this.loaded = true;
    });
  }

  async getGraphQLPersist(): Promise<CloudClient['graphQL']> {
    if (!this.loaded) {
      await this.persistPromise;
    }
    return this.graphQL;
  }

  async getClusterConnection(id: string, noCache = false) {
    const { data } = await this.graphQL.query<GetClusterConnResults>({
      query: GET_CLUSTER_CONN,
      variables: { id },
      fetchPolicy: noCache ? 'network-only' : 'cache-first',
    });
    return data.clusterConnection;
  }
}
