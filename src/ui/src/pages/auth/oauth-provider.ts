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

// Not actually using React in this class, but implementors may offer a component that does.
import type * as React from 'react';

import { FormStructure } from 'app/components';

import { CallbackArgs } from './callback-url';

/** OAuthProviderClient is the interface for OAuth providers such as Auth0 and ORY/Hydra. */
export interface OAuthProviderClient {
  /** refetchToken will get the OAuthProvider to refetch and store the token. */
  refetchToken(): void;

  /** handleToken will get the token wherever it's stored by the OAuthProvider and pass it to the callback. */
  handleToken(): Promise<CallbackArgs>;

  /** getPasswordLoginFlow returns the form structure for logging in. */
  getPasswordLoginFlow(): Promise<FormStructure>;

  /** getResetPasswordFlow returns the form to reset a password. */
  getResetPasswordFlow(): Promise<FormStructure>;

  /** getError retrieves a specific error from the OAuthProvider's server. */
  getError(): Promise<FormStructure>;

  isInvitationEnabled(): boolean;

  /** If the provider supports invitations and they're enabled, it can return a React component to create them. */
  getInvitationComponent(): React.FC | undefined;

  /** Gets the login buttons for this OAuthProvider. */
  getLoginButtons(): React.ReactNode;

  /** Gets the signup buttons for this OAuthProvider. */
  getSignupButtons(): React.ReactNode;
}
