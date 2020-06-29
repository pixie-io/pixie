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
