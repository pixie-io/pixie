// TODO(nick): This file needs implementation to make gRPC testing possible.
//  For the moment, ignoring most lint and inspection rules.

import { Observable, of as observableOf } from 'rxjs';
// noinspection ES6PreferShortImport
import {
  ClusterConfig, PixieAPIClient, PixieAPIClientAbstract,
} from '../../api';
// noinspection ES6PreferShortImport
import { PixieAPIClientOptions } from '../../types/client-options';
// noinspection ES6PreferShortImport
import { ExecutionStateUpdate, VizierQueryFunc } from '../../vizier-grpc-client';
// noinspection ES6PreferShortImport
import { Status } from '../../types/generated/vizierapi_pb';

// noinspection JSUnusedLocalSymbols
// noinspection ES6PreferShortImport
/* eslint-disable */
export class MockPixieAPIClient implements PixieAPIClientAbstract {
  static readonly DEFAULT_OPTIONS = PixieAPIClient.DEFAULT_OPTIONS;
  readonly options: Required<PixieAPIClientOptions>;

  // eslint-disable-next-line class-methods-use-this
  executeScript(
    cluster: string | ClusterConfig, script: string, funcs?: VizierQueryFunc[],
  ): Observable<ExecutionStateUpdate> {
    return observableOf({} as ExecutionStateUpdate);
  }

  // eslint-disable-next-line class-methods-use-this
  health(cluster: string | ClusterConfig): Observable<Status> {
    return observableOf(new Status().setCode(0));
  }

  // Using the same implementation as the real class here, so that mocking the network requests works.
  // This makes the test more accurate and simpler (doesn't have to fiddle with internals).
  isAuthenticated(): Promise<boolean> {
    return fetch(`${this.options.uri}/authorized`).then((response) => response.status === 200);
  }

  // eslint-disable-next-line @typescript-eslint/no-empty-function
  private constructor(options: PixieAPIClientOptions) {
    this.options = { ... MockPixieAPIClient.DEFAULT_OPTIONS, ...options };
  }

  static async create(
    options: PixieAPIClientOptions = {},
  ) {
    return Promise.resolve(new MockPixieAPIClient(MockPixieAPIClient.DEFAULT_OPTIONS));
  }
}
