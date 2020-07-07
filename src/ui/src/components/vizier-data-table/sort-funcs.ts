import { SortDirection, SortDirectionType } from 'react-virtualized';
import { getDataSortFunc } from 'utils/format-data';
import { DataType, SemanticType } from 'types/generated/vizier_pb';

// Sort funcs for semnatic column types (when the pure data type alone doesn't produce
// the sort result that we want.

// TODO(nserrino): Make the selected percentile configurable and sort on that.
export function quantilesSortFunc(percentile: string, ascending: boolean) {
  return (a, b) => {
    const aNull = !(a && a[percentile] != null) ? 1 : 0;
    const bNull = !(b && b[percentile] != null) ? 1 : 0;

    // Nulls come last.
    if (aNull || bNull) {
      return aNull - bNull;
    }

    const result = a[percentile] - b[percentile];
    return (ascending ? result : -result);
  };
}

export function getSortFunc(dataKey: string, type: DataType, semanticType: SemanticType, direction: SortDirectionType) {
  const ascending = direction === SortDirection.ASC;

  let f;
  if (semanticType === SemanticType.ST_QUANTILES) {
    // TODO(nserrino): Make this percentile configurable.
    f = quantilesSortFunc('p99', ascending);
  } else {
    f = getDataSortFunc(type, ascending);
  }
  return (a, b) => f(a[dataKey], b[dataKey]);
}
