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

/**
 * Options object to pass as the `options` argument to @link{PixieAPIClient#create}.
 * Default behavior is to connect to Pixie Cloud.
 */
export interface PixieAPIClientOptions {
  /**
   * Access token. Required for everyone except Pixie Cloud's Live UI code (which uses withCredentials and passes '').
   */
  apiKey: string;
  /**
   * Where the Pixie API is hosted.
   * Includes the protocol, host, port (if needed), and path.
   * Defaults to window.location.origin, which on Pixie Cloud means https://withpixie.ai.
   */
  uri?: string;
  /**
   * A method to invoke when an API request is denied due to a lack of authorization.
   * @default noop
   */
  onUnauthorized?: (errorMessage?: string) => void;
  /**
   * The authToken, if set, is used to enable authentication in embedded mode.
   */
  authToken?: string;
}
