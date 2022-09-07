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

// TODO(nick): This file needs implementation to make gRPC testing possible.
//  For the moment, ignoring most lint and inspection rules.

import fetch from 'cross-fetch';
import { Observable, of as observableOf } from 'rxjs';

import {
  ClusterConfig, PixieAPIClient, PixieAPIClientAbstract, PixieAPIClientOptions,
  ExecutionStateUpdate, VizierQueryFunc, ExecuteScriptOptions, VizierQueryError,
} from 'app/api';
import { Status } from 'app/types/generated/vizierapi_pb';

// noinspection JSUnusedLocalSymbols
// noinspection ES6PreferShortImport
/* eslint-disable */
export class MockPixieAPIClient implements PixieAPIClientAbstract {
  static readonly DEFAULT_OPTIONS = PixieAPIClient.DEFAULT_OPTIONS;
  readonly options: Required<PixieAPIClientOptions>;

  // eslint-disable-next-line class-methods-use-this
  executeScript(
    cluster: ClusterConfig, script: string, opts: ExecuteScriptOptions, funcs?: VizierQueryFunc[],
  ): Observable<ExecutionStateUpdate> {
    return observableOf({} as ExecutionStateUpdate);
  }

  // eslint-disable-next-line class-methods-use-this
  health(cluster: ClusterConfig): Observable<Status> {
    return observableOf(new Status().setCode(0));
  }

  generateOTelExportScript(cluster: ClusterConfig, script: string): Promise<string | VizierQueryError> {
    return new Promise<string | VizierQueryError>((resolve) => { resolve('') });
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
    options: PixieAPIClientOptions = { apiKey: '' },
  ) {
    return Promise.resolve(new MockPixieAPIClient({...MockPixieAPIClient.DEFAULT_OPTIONS, ...options}));
  }
}
