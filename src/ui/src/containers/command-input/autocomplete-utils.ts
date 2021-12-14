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

import { StatusGroup } from 'app/components';
import { GQLAutocompleteEntityKind } from 'app/types/schema';

export type EntityType = 'AEK_UNKNOWN' | 'AEK_POD' | 'AEK_SVC' | 'AEK_SCRIPT' | 'AEK_NAMESPACE' | 'AEK_NODE';

// Converts a vixpb.PXType to an entityType that is accepted by autocomplete.
export function pxTypeToEntityType(pxType: string): GQLAutocompleteEntityKind {
  switch (pxType) {
    case 'PX_SERVICE':
      return GQLAutocompleteEntityKind.AEK_SVC;
    case 'PX_POD':
      return GQLAutocompleteEntityKind.AEK_POD;
    case 'PX_NAMESPACE':
      return GQLAutocompleteEntityKind.AEK_NAMESPACE;
    case 'PX_NODE':
      return GQLAutocompleteEntityKind.AEK_NODE;
    default:
      return GQLAutocompleteEntityKind.AEK_UNKNOWN;
  }
}

export function entityTypeToString(entityType: GQLAutocompleteEntityKind): string {
  switch (entityType) {
    case GQLAutocompleteEntityKind.AEK_SVC:
      return 'svc';
    case GQLAutocompleteEntityKind.AEK_SCRIPT:
      return 'script';
    case GQLAutocompleteEntityKind.AEK_POD:
      return 'pod';
    case GQLAutocompleteEntityKind.AEK_NAMESPACE:
      return 'ns';
    case GQLAutocompleteEntityKind.AEK_NODE:
      return 'node';
    default:
      return '';
  }
}

export function entityStatusGroup(entityState: string): StatusGroup {
  switch (entityState) {
    case 'AES_TERMINATED':
      return 'unknown';
    case 'AES_FAILED':
      return 'unhealthy';
    case 'AES_RUNNING':
      return 'healthy';
    case 'AES_PENDING':
      return 'pending';
    default:
      return 'unknown';
  }
}
