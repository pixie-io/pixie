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

export type Token = string;
// OAuthProviderClient is the interface for OAuth providers such as Auth0 and ORY/Hydra.
export abstract class OAuthProviderClient {
  // loginRequest starts the login process for the OAuthProvider by redirecting the window.
  abstract loginRequest(): void;

  // SignupRequest starts the signup process for the OAuthProvider by redirecting the window.
  abstract signupRequest(): void;

  // handleToken will get the token wherever it's stored by the OAuthProvider and pass it to the callback.
  abstract handleToken(): Promise<Token>;
}
