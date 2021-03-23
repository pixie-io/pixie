/* eslint-disable max-classes-per-file */

import { Observable, from } from 'rxjs';
import { switchMap } from 'rxjs/operators';
import { containsMutation } from 'utils/pxl';
import {
  ExecutionStateUpdate,
  VizierGRPCClient,
  VizierQueryFunc,
} from './vizier-grpc-client';
import { CloudClient } from './cloud-gql-client';
import { PixieAPIClientOptions } from './types/client-options';
import { Status } from './types/generated/vizierapi_pb';
import { UserSettings } from './user-settings';

/**
 * When calling `PixieAPIClient.create`, this specifies which clusters to connect to, and any special configuration for
 * each of those connections as needed.
 */
export interface ClusterConfig {
  id: string;
  /**
   * If provided, this overrides the endpoint for connections to the GRPC API on a cluster.
   * This includes the protocol, the host, and optionally the port. For example, `https://pixie.example.com:1234`.
   */
  passthroughClusterAddress?: string | undefined;
  /**
   * If true, passes an HTTP Authorization header as part of each request to gRPC services.
   */
  attachCredentials?: boolean;
}

export abstract class PixieAPIClientAbstract {
  static readonly DEFAULT_OPTIONS: Required<PixieAPIClientOptions>;

  readonly options: Required<PixieAPIClientOptions>;

  abstract health(cluster: string|ClusterConfig): Observable<Status>;

  abstract executeScript(
    cluster: string|ClusterConfig,
    script: string,
    funcs?: VizierQueryFunc[],
  ): Observable<ExecutionStateUpdate>;

  abstract isAuthenticated(): Promise<boolean>;
}

/**
 * API client library for [Pixie](https://pixielabs.ai).
 * See the [documentation](https://docs.pixielabs.ai/reference/api/overview/) for a complete reference.
 */
export class PixieAPIClient extends PixieAPIClientAbstract {
  /**
   * By default, a new Pixie API Client assumes a matching origin between the UI and API, that a direct connection is
   * sufficient on the current network, and that authentication is being checked elsewhere.
   */
  static readonly DEFAULT_OPTIONS: Required<PixieAPIClientOptions> = Object.freeze({
    uri: `${window.location.origin}/api` || 'https://work.withpixie.ai/api',
    onUnauthorized: () => {},
  });

  private gqlClient: CloudClient;

  private clusterConnections: Map<string, VizierGRPCClient>;

  /**
   * Builds an API client to speak to Pixie's gRPC and GraphQL backends. The former connects to specific clusters;
   * the latter connects to Pixie Cloud.
   *
   * @param options Set these to change the client's behavior or to listen for authorization errors.
   */
  static async create(
    options: PixieAPIClientOptions = {},
  ): Promise<PixieAPIClient> {
    const client = new PixieAPIClient({ ...PixieAPIClient.DEFAULT_OPTIONS, ...options });
    return client.init();
  }

  private constructor(
    readonly options: Required<PixieAPIClientOptions>,
  ) {
    super();
  }

  private async init(): Promise<PixieAPIClient> {
    this.gqlClient = new CloudClient(this.options);
    await this.gqlClient.getGraphQLPersist();
    this.clusterConnections = new Map();

    return this;
  }

  // Note: this doesn't check if the client already exists, and clobbers any existing client.
  private async createVizierClient(cluster: ClusterConfig) {
    const { ipAddress, token } = await this.gqlClient.getClusterConnection(cluster.id, true);
    const client = new VizierGRPCClient(
      cluster.passthroughClusterAddress ?? ipAddress,
      token,
      cluster.id,
      cluster.attachCredentials ?? false,
    );

    /*
     * TODO(nick): Port in the `retry` logic from VizierGRPCClientContext. It should show loading state and keep no more
     *  than two of these connected at a time (due to the HTTP concurrent connections limitation per host).
     *  VizierGRPCClientContext also provides health state and executeScript; we already do that here.
     *  The actual context in the consumer, then, would only need to track the cluster ID instead of the client details.
     */

    // Note that this doesn't currently clean up clients that haven't been used in a while, so a particularly long
    // user session could hold onto a large number of stale connections. A simple page refresh drops them all.
    // If this becomes a problem, limit the number of clients and rotate out those that haven't been used in a while.
    this.clusterConnections.set(cluster.id, client);
    return client;
  }

  private async getClusterClient(cluster: string|ClusterConfig) {
    let id: string;
    let passthroughClusterAddress: string;
    let attachCredentials = false;

    if (typeof cluster === 'string') {
      id = cluster;
    } else {
      ({ id, passthroughClusterAddress, attachCredentials } = cluster);
    }

    return this.clusterConnections.has(id)
      ? Promise.resolve(this.clusterConnections.get(id))
      : this.createVizierClient({ id, passthroughClusterAddress, attachCredentials });
  }

  // TODO(nick): Once the authentication model settles down, make this easier to use outside of the browser.
  /**
   * Checks whether the current cookies include a valid authentication token.
   * Checks by querying a purpose-built endpoint, to be certain the user really is authenticated.
   */
  isAuthenticated(): Promise<boolean> {
    return fetch(`${this.options.uri}/authorized`).then((response) => response.status === 200);
  }

  /**
   * Creates a stream that listens for the health of the cluster and the API client's connection to it.
   * This is an Observable, so don't forget to unsubscribe when you're done with it.
   * @param cluster Which cluster to use. Either just its ID, or a full config. If that cluster has previously been
   *        connected in this session, that connection will be reused without changing its configuration.
   */
  health(cluster: string|ClusterConfig): Observable<Status> {
    return from(this.getClusterClient(cluster))
      .pipe(switchMap((client) => client.health()));
  }

  /**
   * Asks a connected cluster to run a PxL script. Returns an event stream that updates at each stage of execution.
   *
   * A typical event stream might look something like this:
   * - start
   * - metadata
   * - mutation info (if the script uses tracepoints)
   * - data
   * - stats
   *
   * @param cluster Which cluster to use. Either just its ID, or a full config. If that cluster has previously been
   *        connected in this session, that connection will be reused without changing its configuration.
   * @param script The source code of the script to be compiled and executed; whitespace and all.
   * @param funcs Descriptions of which functions in the script to run, and what to do with their output.
   */
  executeScript(
    cluster: string|ClusterConfig,
    script: string, funcs: VizierQueryFunc[] = [],
  ): Observable<ExecutionStateUpdate> {
    const hasMutation = containsMutation(script);
    return from(this.getClusterClient(cluster))
      .pipe(switchMap((client) => client.executeScript(script, funcs, hasMutation)));
  }

  // Convenience: curry methods from the GQL client.
  // TSDoc comments are copy-pasted to avoid an extra click through @see.

  /** Fetches a list of all available clusters. */
  listClusters = () => this.gqlClient.listClusters();

  /**
   * Fetches a list of control planes for currently-available clusters.
   */
  getClusterControlPlanePods = () => this.gqlClient.getClusterControlPlanePods();

  /** Creates a Pixie API key, then returns its ID. */
  createAPIKey = () => this.gqlClient.createAPIKey();

  /** Deletes a Pixie API key with the given ID. */
  deleteAPIKey = (id: string) => this.gqlClient.deleteAPIKey(id);

  /** Fetches a list of accessible Pixie API keys. Results update at most once every 2 seconds. */
  listAPIKeys = () => this.gqlClient.listAPIKeys();

  /** Creates a cluster deployment key, then returns its ID. */
  createDeploymentKey = () => this.gqlClient.createDeploymentKey();

  /** Deletes a cluster deployment key with the given ID. */
  deleteDeploymentKey = (id: string) => this.gqlClient.deleteDeploymentKey(id);

  /** Fetches a list of accessible cluster deployment keys. Results update at most once every 2 seconds. */
  listDeploymentKeys = () => this.gqlClient.listDeploymentKeys();

  /**
   * Creates a function that can suggest complete commands for a cluster, such as a script to execute and its args.
   * For an example of this in use, check out CommandAutocomplete in @pixie-labs/components
   */
  getAutocompleteSuggester = (clusterUID: string) => this.gqlClient.getAutocompleteSuggester(clusterUID);

  /**
   * Creates a function that can suggest entity names (such as script IDs) based on a partial input.
   * For an example of this in use, check out Breadcrumbs in @pixie-labs/components
   */
  getAutocompleteFieldSuggester = (clusterUID: string) => this.gqlClient.getAutocompleteFieldSuggester(clusterUID);

  /**
   * Fetches the upstream value of a specific user setting.
   * Adapter libraries may wish to leverage localStorage in addition to Apollo's cache, to get the
   * last-known value synchronously on page load. @pixie-labs/api-react is an example of this.
   */
  getSetting = (key: keyof UserSettings) => this.gqlClient.getSetting(key);

  /**
   * Writes a user setting upstream.
   * As with getSetting, adapter libraries may wish to use localStorage in combination with this.
   */
  setSetting = (key: keyof UserSettings, value: UserSettings[typeof key]) => this.gqlClient.setSetting(key, value);

  /**
   * Implementation detail for adapters like @pixie-labs/api-react.
   * Do not use directly unless writing such an adapter.
   *
   * Provides the internal CloudClient, which has a graphQL property.
   * That property is an ApolloClient, with which GraphQL queries can be run directly.
   * TODO(nick): Once VizierGRPCClientContext logic moves, only provide the ApolloClient itself.
   *
   * @internal
   */
  getCloudGQLClientForAdapterLibrary() {
    return this.gqlClient;
  }
}
