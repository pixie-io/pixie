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
import {
  API_KEY_QUERIES,
  AUTOCOMPLETE_QUERIES,
  CLUSTER_QUERIES,
  DEPLOYMENT_KEY_QUERIES, USER_QUERIES,
} from 'gql-queries';
import fetch from 'cross-fetch';
import { PixieAPIClientOptions } from './types/client-options';
import {
  GQLAPIKey,
  GQLAutocompleteActionType,
  GQLAutocompleteEntityKind,
  GQLAutocompleteResult, GQLAutocompleteSuggestion, GQLClusterInfo,
  GQLDeploymentKey, GQLUserInfo,
} from './types/schema';
import { DEFAULT_USER_SETTINGS, UserSettings } from './user-settings';

// Apollo link that adds cookies in the request.
const makeCloudAuthLink = (opts: PixieAPIClientOptions) => setContext((_, { headers }) => ({
  headers: {
    ...headers,
    // NOTE: apiKey is required in the interface because every consumer EXCEPT Pixie's web UI must provide it.
    // Pixie's web UI provides the empty string to indicate that it's using credentials instead.
    // If any other consumer tries to do the same thing in a browser, CORS will block the request on the API side.
    ...(opts.apiKey ? { 'pixie-api-key': opts.apiKey } : { withCredentials: true }),
  },
}));

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = (on401: (errorMessage?: string) => void) => onError(({ networkError }) => {
  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    on401(networkError.message);
  }
});

export interface ClusterConnection {
  ipAddress: string;
  token: string;
}

export interface GetClusterConnResults {
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
      connectToDevTools: globalThis.process?.env && process.env.NODE_ENV === 'development',
      cache: this.cache,
      link: ApolloLink.from([
        makeCloudAuthLink(opts),
        loginRedirectLink(opts.onUnauthorized ?? (() => {})),
        createHttpLink({ uri: `${opts.uri}/graphql`, fetch }),
      ]),
    });

    // On NodeJS, there is no localStorage. However, we still want to cache at runtime, so use a Map.
    // In a browser, we can use localStorage normally.
    const storage = globalThis.localStorage ?? (() => {
      const map = new Map<string, any>();
      return {
        getItem: (k: string) => map.get(k),
        setItem: (k: string, v: any) => {
          map.set(k, v);
        },
        removeItem: (k: string) => {
          map.delete(k);
        },
      };
    })();

    this.persistPromise = persistCache({
      cache: this.cache,
      storage,
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

  /**
   * Implementation detail for forming a connection to a cluster for health check and script execution requests.
   */
  async getClusterConnection(id: string, noCache = false): Promise<ClusterConnection> {
    const { data } = await this.graphQL.query<GetClusterConnResults>({
      query: CLUSTER_QUERIES.GET_CLUSTER_CONN,
      variables: { id },
      fetchPolicy: noCache ? 'network-only' : 'cache-first',
    });
    return data.clusterConnection;
  }

  /** Fetches a list of all available clusters. */
  async listClusters(): Promise<GQLClusterInfo[]> {
    const { data } = await this.graphQL.query<{ clusters: GQLClusterInfo[] }>({
      query: CLUSTER_QUERIES.LIST_CLUSTERS,
      fetchPolicy: 'network-only',
    });
    return data.clusters;
  }

  /**
   * Fetches a list of control planes for currently-available clusters.
   */
  async getClusterControlPlanePods(): Promise<GQLClusterInfo[]> {
    const { data } = await this.graphQL.query<{ clusters: GQLClusterInfo[] }>({
      query: CLUSTER_QUERIES.GET_CLUSTER_CONTROL_PLANE_PODS,
    });
    return data.clusters;
  }

  /** Creates a Pixie API key, then returns its ID. */
  async createAPIKey(): Promise<string> {
    const { data } = await this.graphQL.mutate<{ CreateAPIKey: { id: string } }>({
      mutation: API_KEY_QUERIES.CREATE_API_KEY,
    });
    return data.CreateAPIKey.id;
  }

  /** Deletes a Pixie API key with the given ID. */
  async deleteAPIKey(id: string): Promise<void> {
    await this.graphQL.mutate<void, { id: string }>({
      mutation: API_KEY_QUERIES.DELETE_API_KEY,
      variables: { id },
    });
    // Nothing to return here. Just wait for the promise to resolve or reject.
  }

  /** Fetches a list of accessible Pixie API keys. */
  async listAPIKeys(): Promise<GQLAPIKey[]> {
    const { data } = await this.graphQL.query<{ apiKeys: GQLAPIKey[] }>({
      query: API_KEY_QUERIES.LIST_API_KEYS,
      fetchPolicy: 'no-cache',
    });
    return data.apiKeys;
  }

  /** Creates a cluster deployment key, then returns its ID. */
  async createDeploymentKey(): Promise<string> {
    const { data } = await this.graphQL.mutate<{ CreateDeploymentKey: { id: string } }>({
      mutation: DEPLOYMENT_KEY_QUERIES.CREATE_DEPLOYMENT_KEY,
    });
    return data.CreateDeploymentKey.id;
  }

  /** Deletes a cluster deployment key with the given ID. */
  async deleteDeploymentKey(id: string): Promise<void> {
    await this.graphQL.mutate<void, { id: string }>({
      mutation: DEPLOYMENT_KEY_QUERIES.DELETE_DEPLOYMENT_KEY,
      variables: { id },
    });
    // Nothing to return here. Just wait for the promise to resolve or reject.
  }

  /** Fetches a list of accessible cluster deployment keys. */
  async listDeploymentKeys(): Promise<GQLDeploymentKey[]> {
    const { data } = await this.graphQL.query<{ deploymentKeys: GQLDeploymentKey[] }>({
      query: DEPLOYMENT_KEY_QUERIES.LIST_DEPLOYMENT_KEYS,
      fetchPolicy: 'no-cache',
    });
    return data.deploymentKeys;
  }

  /**
   * Creates a function that can suggest complete commands for a cluster, such as a script to execute and its args.
   * For an example of this in use, check out CommandAutocomplete in @pixie-labs/components
   */
  getAutocompleteSuggester(
    clusterUID: string,
  ): (input: string, cursor: number, action: GQLAutocompleteActionType) => Promise<GQLAutocompleteResult> {
    return (input, cursor, action) => this.graphQL.query<{ autocomplete: GQLAutocompleteResult }>({
      query: AUTOCOMPLETE_QUERIES.AUTOCOMPLETE,
      fetchPolicy: 'network-only',
      variables: {
        input,
        cursor,
        action,
        clusterUID,
      },
    }).then(
      (results) => results.data.autocomplete,
    );
  }

  /**
   * Creates a function that can suggest entity names (such as script IDs) based on a partial input.
   * For an example of this in use, check out Breadcrumbs in @pixie-labs/components
   */
  getAutocompleteFieldSuggester(
    clusterUID: string,
  ): (input: string, kind: GQLAutocompleteEntityKind) => Promise<GQLAutocompleteSuggestion[]> {
    return (input, kind) => this.graphQL.query<{ autocompleteField: GQLAutocompleteSuggestion[] }>({
      query: AUTOCOMPLETE_QUERIES.FIELD,
      fetchPolicy: 'network-only',
      variables: {
        input,
        kind,
        clusterUID,
      },
    }).then(
      (results) => results.data.autocompleteField,
    );
  }

  /**
   * Fetches the upstream value of a specific user setting.
   * Adapter libraries may wish to leverage localStorage in addition to Apollo's cache, to get the
   * last-known value synchronously on page load. @pixie-labs/api-react is an example of this.
   */
  async getSetting(key: keyof UserSettings): Promise<UserSettings[typeof key]> {
    const { data } = await this.graphQL.query<{ userSettings: Array<Record<string, string>> }>({
      query: USER_QUERIES.GET_ALL_USER_SETTINGS,
      fetchPolicy: 'cache-first',
    });
    if (data.userSettings[key] == null) return DEFAULT_USER_SETTINGS[key];
    try {
      return JSON.parse(data.userSettings[key]);
    } catch {
      return data.userSettings[key];
    }
  }

  /**
   * Writes a user setting upstream.
   * As with getSetting, adapter libraries may wish to use localStorage in combination with this.
   */
  async setSetting(key: keyof UserSettings, value: UserSettings[typeof key]): Promise<void> {
    await this.graphQL.mutate({
      mutation: USER_QUERIES.SAVE_USER_SETTING,
      variables: { key, value: JSON.stringify(value) },
    });
    // Nothing to return here. Just wait for the promise to resolve or reject.
  }

  async getUserInfo(): Promise<GQLUserInfo> {
    const { data } = await this.graphQL.query<{ user: GQLUserInfo }>({
      query: USER_QUERIES.GET_USER_INFO,
      fetchPolicy: 'network-only',
    });
    return data.user;
  }
}
