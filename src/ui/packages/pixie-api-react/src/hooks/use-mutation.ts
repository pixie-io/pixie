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

import { useMutation as useMutationInternal } from '@apollo/client/react';
import { OperationVariables } from '@apollo/client/core';
import { MutationHookOptions, MutationTuple } from '@apollo/client/react/types/types';

import { DocumentNode } from 'graphql';
import { TypedDocumentNode } from '@graphql-typed-document-node/core';

/**
 * useMutation is a simple proxy around Apollo's useQuery that hooks into existing
 * apollo context management in the API.
 */
export function useMutation<TData = any, TVariables = OperationVariables>(
  mutation: DocumentNode | TypedDocumentNode<TData, TVariables>,
  options?: MutationHookOptions<TData, TVariables>): MutationTuple<TData, TVariables> {
  return useMutationInternal(mutation, options);
}
