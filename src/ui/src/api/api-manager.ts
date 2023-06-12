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

import { PixieAPIClientOptions } from '.';
import { PixieAPIClientAbstract, PixieAPIClient } from './api-client';

/**
 * Manages a singleton PixieAPIClient so that it can be used and configured both inside and outside of React's scope.
 * This allows logic involving authentication to work anywhere in the app, even before React starts up.
 */
export class PixieAPIManager {
  private static _uri: string;

  private static _apiKey = '';

  private static _authToken: string;

  private static _onUnauthorized: (errorMessage?: string) => void;

  private static _instance: PixieAPIClientAbstract;

  public static get instance(): PixieAPIClientAbstract {
    if (!PixieAPIManager._instance) PixieAPIManager.reinit();
    return PixieAPIManager._instance;
  }

  private static reinit() {
    const opts: PixieAPIClientOptions = { apiKey: '' };
    if (PixieAPIManager._uri != null) opts.uri = PixieAPIManager._uri;
    if (PixieAPIManager._apiKey != null) opts.apiKey = PixieAPIManager._apiKey;
    if (PixieAPIManager._authToken != null) opts.authToken = PixieAPIManager._authToken;
    if (PixieAPIManager._onUnauthorized != null) opts.onUnauthorized = PixieAPIManager._onUnauthorized;
    PixieAPIManager._instance = PixieAPIClient.create(opts);
    // See api-context.tsx for why this exists
    if (window.setApiContextUpdatesFromOutsideReact) window.setApiContextUpdatesFromOutsideReact((prev) => prev + 1);
  }

  public static get uri() { return PixieAPIManager._uri; }

  public static set uri(uri: string) {
    if (PixieAPIManager._uri !== uri) {
      PixieAPIManager._uri = uri;
      PixieAPIManager.reinit();
    }
  }

  public static get apiKey() { return PixieAPIManager._apiKey; }

  public static set apiKey(apiKey: string) {
    if (PixieAPIManager._apiKey !== (apiKey ?? '')) {
      PixieAPIManager._apiKey = apiKey ?? '';
      PixieAPIManager.reinit();
    }
  }

  public static get authToken() { return PixieAPIManager._authToken; }

  public static set authToken(authToken: string) {
    if (PixieAPIManager._authToken !== authToken) {
      PixieAPIManager._authToken = authToken;
      PixieAPIManager.reinit();
    }
  }

  public static get onUnauthorized() { return PixieAPIManager._onUnauthorized; }

  public static set onUnauthorized(onUnauthorized: (errorMessage?: string) => void) {
    if (PixieAPIManager._onUnauthorized != onUnauthorized) {
      PixieAPIManager._onUnauthorized = onUnauthorized ?? (() => {});
      PixieAPIManager.reinit();
    }
  }
}
