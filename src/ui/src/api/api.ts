/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* eslint-disable max-classes-per-file */

import { Observable, from } from 'rxjs';
import { switchMap } from 'rxjs/operators';
import { containsMutation } from 'app/utils/pxl';
import fetch from 'cross-fetch';
import { Status } from 'app/types/generated/vizierapi_pb';
import {
  ExecutionStateUpdate,
  VizierGRPCClient,
  VizierQueryFunc,
} from './vizier-grpc-client';
import { PixieAPIClientOptions } from './api-options';
import { CloudClient } from './cloud-gql-client';
import { GetCSRFCookie } from '../pages/auth/utils';

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
    apiKey: '',
    uri: `${globalThis?.location?.origin || 'https://work.withpixie.ai'}/api`,
    onUnauthorized: () => {},
    authToken: '',
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
    options: PixieAPIClientOptions,
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
      // If in embed mode, we should always use the auth token with bearer auth.
      this.options.authToken ? this.options.authToken : token,
      cluster.id,
      (this.options.authToken ? false : cluster.attachCredentials ?? false),
    );

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
    return fetch(`${this.options.uri}/authorized`,
      {
        headers: {
          ...{
            'x-csrf': GetCSRFCookie(),
          },
          ...(this.options.authToken
            ? { authorization: `Bearer ${this.options.authToken}`, 'X-Use-Bearer': 'true' } : {}),
        },
      }).then((response) => response.status === 200);
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

  /**
   * Implementation detail for adapters.
   * Do not use directly unless writing such an adapter.
   *
   * Provides the internal CloudClient, which has a graphQL property.
   * That property is an ApolloClient, with which GraphQL queries can be run directly.
   *
   * @internal
   */
  getCloudClient(): CloudClient {
    return this.gqlClient;
  }
}
