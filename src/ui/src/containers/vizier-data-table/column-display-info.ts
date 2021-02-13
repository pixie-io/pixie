import { SelectedPercentile } from 'pixie-components';
import { DataType, Relation, SemanticType } from 'types/generated/vizierapi_pb';

export interface QuantilesDisplayState {
  selectedPercentile: SelectedPercentile;
}

type TypeSpecificDisplayState = QuantilesDisplayState | {};

function defaultDisplayState(st: SemanticType) {
  if (st === SemanticType.ST_QUANTILES || st === SemanticType.ST_DURATION_NS_QUANTILES) {
    return { selectedPercentile: 'p99' };
  }
  return {};
}

export interface ColumnDisplayInfo {
  columnName: string;
  baseTitle: string;
  type: DataType;
  semanticType: SemanticType;
  displayState: TypeSpecificDisplayState;
}

export function displayInfoFromColumn(col: Relation.ColumnInfo): ColumnDisplayInfo {
  const st = col.getColumnSemanticType();
  const displayInfo: ColumnDisplayInfo = {
    columnName: col.getColumnName(),
    baseTitle: col.getColumnName(),
    type: col.getColumnType(),
    semanticType: st,
    displayState: defaultDisplayState(st),
  };
  return displayInfo;
}

export function titleFromInfo(col: ColumnDisplayInfo) {
  if (col.semanticType === SemanticType.ST_QUANTILES || col.semanticType === SemanticType.ST_DURATION_NS_QUANTILES) {
    const quantilesState = col.displayState as QuantilesDisplayState;
    const selectedPercentile = quantilesState.selectedPercentile || 'p99';
    return `${col.baseTitle} (${selectedPercentile})`;
  }
  return col.baseTitle;
}
