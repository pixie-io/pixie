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

import * as React from 'react';

// eslint-disable-next-line @typescript-eslint/no-unused-vars
export function useCreateOrgExtras(isCreating: boolean): {
  /** Tasks to run when the Create button is pressed, before actually creating the org */
  beforeCreate: () => Promise<void>,
  /** Tasks to run after the org is created and the auth token is re-fetched, but before redirecting */
  afterCreate: () => Promise<void>,
  /** Any additional UI to present right above the Create button, such as a checkbox */
  infixComponent: React.ReactNode,
  /** If beforeCreate or afterCreate are running, use this to tell the user what they're waiting on */
  loadingText?: string,
  /** Allows disabling the Create button if this component adds any extra conditions that need to be met first */
  valid: boolean,
} {
  const beforeCreate = React.useCallback(async () => {}, []);
  const afterCreate = React.useCallback(async () => {}, []);
  const infixComponent = null;
  const loadingText = '';

  return React.useMemo(() => ({
    beforeCreate,
    afterCreate,
    infixComponent,
    loadingText,
    valid: true,
  }), [beforeCreate, afterCreate, infixComponent, loadingText]);
}
