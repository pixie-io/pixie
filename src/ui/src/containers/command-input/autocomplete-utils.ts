import { StatusGroup } from '@pixie-labs/components';
import { GQLAutocompleteEntityKind } from '@pixie-labs/api';

export type EntityType = 'AEK_UNKNOWN' | 'AEK_POD' | 'AEK_SVC' | 'AEK_SCRIPT' | 'AEK_NAMESPACE';

// Converts a vixpb.PXType to an entityType that is accepted by autocomplete.
export function pxTypeToEntityType(pxType: string): GQLAutocompleteEntityKind {
  switch (pxType) {
    case 'PX_SERVICE':
      return GQLAutocompleteEntityKind.AEK_SVC;
    case 'PX_POD':
      return GQLAutocompleteEntityKind.AEK_POD;
    case 'PX_NAMESPACE':
      return GQLAutocompleteEntityKind.AEK_NAMESPACE;
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
