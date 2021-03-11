import {
  ApolloClient,
  InMemoryCache,
  NormalizedCacheObject,
  ApolloLink,
  createHttpLink,
  ServerError,
} from '@apollo/client/core';
import { setContext } from '@apollo/client/link/context';
import { onError } from '@apollo/client/link/error';
import { persistCache } from 'apollo3-cache-persist';
import { CLUSTER_QUERIES } from 'gql-queries';
import { fetch } from 'whatwg-fetch';
import { PixieAPIClientOptions } from 'types/client-options';

// Apollo link that adds cookies in the request.
const cloudAuthLink = setContext((_, { headers }) => ({
  headers: {
    ...headers,
    withCredentials: true,
  },
}));

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = (on401: (errorMessage?: string) => void) => onError(({ networkError }) => {
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

export class CloudClient {
  graphQL: ApolloClient<NormalizedCacheObject>;

  private readonly persistPromise: Promise<void>;

  private readonly cache: InMemoryCache;

  private loaded = false;

  constructor(opts: PixieAPIClientOptions) {
    this.cache = new InMemoryCache({
      typePolicies: {
        AutocompleteResult: {
          keyFields: ['formattedInput'],
        },
      },
    });
    this.graphQL = new ApolloClient({
      connectToDevTools: process?.env && process.env.NODE_ENV === 'development',
      cache: this.cache,
      link: ApolloLink.from([
        cloudAuthLink,
        loginRedirectLink(opts.onUnauthorized ?? (() => {})),
        createHttpLink({ uri: `${opts.uri}/graphql`, fetch }),
      ]),
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
      query: CLUSTER_QUERIES.GET_CLUSTER_CONN,
      variables: { id },
      fetchPolicy: noCache ? 'network-only' : 'cache-first',
    });
    return data.clusterConnection;
  }
}
