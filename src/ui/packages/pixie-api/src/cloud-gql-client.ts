import { InMemoryCache, NormalizedCacheObject } from 'apollo-cache-inmemory';
import { persistCache } from 'apollo-cache-persist';
import { ApolloClient } from 'apollo-client';
import { ApolloLink } from 'apollo-link';
import { setContext } from 'apollo-link-context';
import { onError } from 'apollo-link-error';
import { createHttpLink } from 'apollo-link-http';
import { ServerError } from 'apollo-link-http-common';
import gql from 'graphql-tag';
import { fetch } from 'whatwg-fetch';

// Apollo link that adds cookies in the request.
const cloudAuthLink = setContext((_, { headers }) => ({
  headers: {
    ...headers,
    withCredentials: true,
  },
}));

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = (on401: (errorMessage: string) => void) => onError(({ networkError }) => {
  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    on401(networkError.message);
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
  graphQl: ApolloClient<NormalizedCacheObject>;

  private readonly persistPromise: Promise<void>;

  private readonly cache: InMemoryCache;

  private loaded = false;

  constructor(private readonly on401: (errorMessage: string) => void) {
    this.cache = new InMemoryCache();
    this.graphQl = new ApolloClient({
      cache: this.cache,
      link: ApolloLink.from([
        cloudAuthLink,
        loginRedirectLink(on401),
        createHttpLink({ uri: '/api/graphql', fetch }),
      ]),
    });

    this.persistPromise = persistCache({
      cache: this.cache,
      storage: window.localStorage,
    }).then(() => {
      this.loaded = true;
    });
  }

  async getGraphQLPersist(): Promise<CloudClient['graphQl']> {
    if (!this.loaded) {
      await this.persistPromise;
    }
    return this.graphQl;
  }

  async getClusterConnection(id: string, noCache = false) {
    const { data } = await this.graphQl.query<GetClusterConnResults>({
      query: GET_CLUSTER_CONN,
      variables: { id },
      fetchPolicy: noCache ? 'network-only' : 'cache-first',
    });
    return data.clusterConnection;
  }
}
