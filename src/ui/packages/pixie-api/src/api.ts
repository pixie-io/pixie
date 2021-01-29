import { ExecutionStateUpdate, VizierGRPCClient, VizierQueryFunc } from 'vizier-grpc-client';
import { CloudClient } from 'cloud-gql-client';
import { Observable } from 'rxjs';
import { Status } from 'types/generated/vizier_pb';
import { containsMutation } from 'utils/pxl';

/**
 * Options object to pass as the `options` argument to @link{PixieAPIClient#create}.
 */
export interface PixieAPIClientOptions {
  /**
   * The domain name to connect to. An HTTPS protocol is assumed and cannot be overridden.
   * By default, it points to the public Pixie Cloud instance.
   * @default withpixie.ai
   */
  host?: string;
  /**
   * The URI after the domain that prefixes all GraphQL queries.
   * For self hosted instances, this will usually remain default unless Pixie's HTTP routes are hidden behind nginx etc.
   * @default /api/graphql
   */
  gqlRootPath?: string;
  /**
   * A method to invoke when an API request is denied due to a lack of authorization.
   * @default noop
   */
  onUnauthorized?: (errorMessage?: string) => void;
}

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

/**
 * API client library for [Pixie](https://pixielabs.ai).
 * See the [documentation](https://docs.pixielabs.ai/using-pixie/using-api/) for a complete reference.
 */
export class PixieAPIClient {
  /**
   * By default, a new Pixie API Client will assume the following:
   * - It is connecting to the public instance of Pixie Cloud
   * - A direct connection will work and no passthrough is required
   * - Logging to the JS console is sufficient for error reporting
   */
  static readonly DEFAULT_OPTIONS: Required<PixieAPIClientOptions> = Object.freeze({
    host: 'withpixie.ai',
    gqlRootPath: '/api/graphql',
    onUnauthorized: () => {},
  });

  private gqlClient: CloudClient;

  private clusterConnections: Map<string, VizierGRPCClient>;

  /**
   * Builds an API client to speak to Pixie's gRPC and GraphQL backends. The former connects to specific clusters;
   * the latter connects to Pixie Cloud.
   *
   * @param authToken API token. Can be obtained in the web UI in the admin area, or by running `px api-key create`.
   * @param clusters List of clusters to which this client will be sending script execution and health check requests.
   * @param options Set these to change the client's behavior or to listen for authorization errors.
   */
  static async create(
    authToken: string,
    clusters: ClusterConfig[],
    options: PixieAPIClientOptions = {},
  ): Promise<PixieAPIClient> {
    const client = new PixieAPIClient(authToken, clusters, { ...PixieAPIClient.DEFAULT_OPTIONS, ...options });
    return client.init();
  }

  private constructor(
    readonly authToken: string,
    readonly clusters: ClusterConfig[],
    readonly options: Required<PixieAPIClientOptions>,
  ) {}

  private async init(): Promise<PixieAPIClient> {
    this.gqlClient = new CloudClient(this.options.onUnauthorized);
    this.clusterConnections = new Map();

    for (const cluster of this.clusters) {
      // eslint-disable-next-line no-await-in-loop
      this.clusterConnections.set(cluster.id, await this.createVizierClient(cluster));
    }

    return this;
  }

  private async createVizierClient(cluster: ClusterConfig) {
    const { ipAddress, token } = await this.gqlClient.getClusterConnection(cluster.id, true);
    return new VizierGRPCClient(
      cluster.passthroughClusterAddress ?? ipAddress,
      token,
      cluster.id,
      cluster.attachCredentials ?? false,
    );
  }

  private checkForClusterClient(clusterID: string) {
    if (!this.clusterConnections.has(clusterID)) {
      throw new Error('Health checks can only run for clusters with an active API client');
    }
  }

  /**
   * Creates a stream that listens for the health of the cluster and the API client's connection to it.
   * This is an Observable, so don't forget to unsubscribe when you're done with it.
   * @param clusterID Which cluster to use - this must be one that was specified in PixieAPIClient.create().
   */
  health(clusterID: string): Observable<Status> {
    this.checkForClusterClient(clusterID);
    return this.clusterConnections.get(clusterID).health();
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
   * @param clusterID Which cluster to use - this must be one that was specified in PixieAPIClient.create().
   * @param script The source code of the script to be compiled and executed; whitespace and all.
   * @param funcs Descriptions of which functions in the script to run, and what to do with their output.
   */
  executeScript(clusterID: string, script: string, funcs: VizierQueryFunc[] = []): Observable<ExecutionStateUpdate> {
    this.checkForClusterClient(clusterID);
    const hasMutation = containsMutation(script);
    return this.clusterConnections.get(clusterID).executeScript(script, funcs, hasMutation);
  }

  /*
   * TODO(nick): Future changes to pull code out to, and to polish, `pixie-api`. In order:
   * - https://www.apollographql.com/docs/react/migrating/apollo-client-3-migration/
   * - Need to copy over src/cloud/api/controller/schema.d.ts and have Arcanist verify that happens, as with proto defs.
   * - Wrap all GQL endpoints can be wrapped directly in this class.
   * - Change all UI code that invokes the API to use `api.ts` to do it.
   * - Create test wrappers for the new API and move all existing tests to use that.
   * - Clean up imports and package.json dependencies.
   *   Double check everything is imported from `pixie-api`, not anything inside it.
   *   There should remain only one Apollo dependency with two uses: `@apollo/client` in api.ts and the new test helper.
   * - Pull logic for some endpoints, like executeScript, up into the API code (anything that every consumer would need)
   * - Documentation, documentation, documentation!
   *   This includes reviewing all code comments related to the API, as some details may have mutated.
   */
}
