import { SortDirection, SortDirectionType } from 'react-virtualized';
import { getDataSortFunc } from 'utils/format-data';
import { SemanticType } from 'types/generated/vizierapi_pb';
import { checkExhaustive } from 'utils/check-exhaustive';
import { ColumnDisplayInfo, QuantilesDisplayState } from './column-display-info';

// Sort funcs for semantic column types (when the pure data type alone doesn't produce
// the sort result that we want.

/**
 * Returns a sorting function that extracts and compares a specific field from the compared objects.
 * Sorts missing (null, undefined) values to the bottom when in ascending order.
 *
 * @param fieldName What field to look at when comparing two objects that both may have that field.
 * @param ascending If true, "bigger" values are sorted to the bottom. If false, they're sorted to the top.
 */
export function fieldSortFunc(fieldName: string, ascending: boolean) {
  return (a, b) => {
    const aVal = a?.[fieldName];
    const bVal = b?.[fieldName];

    // Number(true) returns 1, Number(false) returns 0.
    const aNull = Number(aVal == null);
    const bNull = Number(bVal == null);

    // Nulls come last.
    if (aNull || bNull) {
      return aNull - bNull;
    }

    // The type must be in a const, rather than directly referenced, for checkExhaustive
    // and switch(typeof aVal) to realize they're referring to the same (narrowed) type.
    const aType = typeof aVal;
    const bType = typeof bVal;
    if (aType !== bType) {
      throw new Error(`Cannot compare disparate types '${aType}' and '${bType}' (a=${aVal}; b=${bVal})`);
    }

    let result: number;
    switch (aType) {
      case 'number':
      case 'bigint':
      case 'boolean':
        // Number(true) is 1, Number(false) and Number(null) are 0.
        result = Number(aVal) - Number(bVal);
        break;
      case 'string':
        result = aVal.localeCompare(bVal);
        break;
      case 'object':
      case 'function':
      case 'symbol':
      case 'undefined':
        throw new Error(`Cannot compare values of type '${aType}'`);
      default:
        checkExhaustive(aType);
    }

    return (ascending ? result : -result);
  };
}

export function getSortFunc(display: ColumnDisplayInfo, direction: SortDirectionType) {
  const ascending = direction === SortDirection.ASC;

  let f;
  switch (display.semanticType) {
    case SemanticType.ST_QUANTILES:
    case SemanticType.ST_DURATION_NS_QUANTILES: {
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
    case SemanticType.ST_SCRIPT_REFERENCE:
      f = fieldSortFunc('label', ascending);
      break;
    default:
      f = getDataSortFunc(display.type, ascending);
      break;
  }
  return (a, b) => f(a[display.columnName], b[display.columnName]);
}
