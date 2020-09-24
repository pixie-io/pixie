import { SortDirection, SortDirectionType } from 'react-virtualized';
import { getDataSortFunc } from 'utils/format-data';
import { DataType, SemanticType } from 'types/generated/vizier_pb';
import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

// Sort funcs for semantic column types (when the pure data type alone doesn't produce
// the sort result that we want.

// Sorts an object on a specific field name.
export function fieldSortFunc(fieldName: string, ascending: boolean) {
  return (a, b) => {
    const aNull = !(a && a[fieldName] != null) ? 1 : 0;
    const bNull = !(b && b[fieldName] != null) ? 1 : 0;

    // Nulls come last.
    if (aNull || bNull) {
      return aNull - bNull;
    }

    const result = a[fieldName] - b[fieldName];
    return (ascending ? result : -result);
  };
}

export function getSortFunc(display: ColumnDisplayInfo, direction: SortDirectionType) {
  const ascending = direction === SortDirection.ASC;

  let f;
  switch (display.semanticType) {
    case SemanticType.ST_QUANTILES: {
      const quantilesDisplay = display.displayState as QuantilesDisplayState;
      const selectedPercentile = quantilesDisplay.selectedPercentile || 'p99';
      f = fieldSortFunc(selectedPercentile, ascending);
      break;
    }
    case SemanticType.ST_POD_STATUS:
      f = fieldSortFunc('phase', ascending);
      break;
    case SemanticType.ST_CONTAINER_STATUS:
      f = fieldSortFunc('state', ascending);
      break;
    default:
      f = getDataSortFunc(display.type, ascending);
      break;
  }
  return (a, b) => f(a[display.columnName], b[display.columnName]);
}
