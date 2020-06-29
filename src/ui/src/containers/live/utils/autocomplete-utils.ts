export type EntityType = 'AEK_UNKNOWN' | 'AEK_POD' | 'AEK_SVC' | 'AEK_SCRIPT' | 'AEK_NAMESPACE';

// Converts a vixpb.PXType to an entityType that is accepted by autocomplete.
export function pxTypetoEntityType(pxType: string): EntityType {
  switch (pxType) {
    case 'PX_SERVICE':
      return 'AEK_SVC';
    case 'PX_POD':
      return 'AEK_POD';
    case 'PX_NAMESPACE':
      return 'AEK_NAMESPACE';
    default:
      return 'AEK_UNKNOWN';
  }
}

export function entityTypeToString(entityType: EntityType): string {
  switch (entityType) {
    case 'AEK_SVC':
      return 'svc';
    case 'AEK_SCRIPT':
      return 'script';
    case 'AEK_POD':
      return 'pod';
    case 'AEK_NAMESPACE':
      return 'ns';
    default:
      return '';
  }
}
