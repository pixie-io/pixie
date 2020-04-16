import {Table} from 'common/vizier-grpc-client';
import {CellAlignment, ColumnProps, DataTable, SortState} from 'components/data-table';
import * as React from 'react';
import {Column, DataType} from 'types/generated/vizier_pb';
import {formatFloat64Data, formatInt64Data} from 'utils/format-data';
import {dataFromProto} from 'utils/result-data-utils';

import {createStyles, makeStyles, Theme, withStyles} from '@material-ui/core/styles';

interface VizierDataTableProps {
  table: Table;
}

const AlignmentMap = new Map<DataType, CellAlignment>(
  [
    [DataType.BOOLEAN, 'center'],
    [DataType.INT64, 'end'],
    [DataType.UINT128, 'start'],
    [DataType.FLOAT64, 'end'],
    [DataType.STRING, 'start'],
    [DataType.TIME64NS, 'start'],
    [DataType.DURATION64NS, 'start'],
  ],
);

function getDataRenderer(type: DataType) {
  switch (type) {
    case DataType.FLOAT64:
      return formatFloat64Data;
    case DataType.TIME64NS:
      return (d) => new Date(d).toLocaleString();
    case DataType.INT64:
      return formatInt64Data;
    case DataType.DURATION64NS:
    case DataType.UINT128:
    case DataType.STRING:
    case DataType.BOOLEAN:
    default:
      return (d) => d.toString();
  }
}

function getSortFunc(dataKey: string, type: DataType, direction: 'asc' | 'desc') {
  const dir = direction === 'asc' ? -1 : 1;
  return (a, b) => {
    return a[dataKey] < b[dataKey] ? dir : -dir;
  };
}

function formatRow(row, columnsMap) {
  const out = {};
  for (const key of Object.keys(row)) {
    const column = columnsMap.get(key);
    const renderer = getDataRenderer(column.type);
    out[key] = renderer(row[key]);
  }
  return out;
}

export const VizierDataTable = (props: VizierDataTableProps) => {
  const { table } = props;
  const [rows, setRows] = React.useState([]);
  React.useEffect(() => {
    setRows(dataFromProto(table.relation, table.data));
  }, [table.relation, table.data]);

  const columnsMap = React.useMemo(() => {
    const map = new Map();
    for (const col of table.relation.getColumnsList()) {
      const name = col.getColumnName();
      map.set(name, {
        type: col.getColumnType(),
        dataKey: col.getColumnName(),
        label: col.getColumnName(),
        align: AlignmentMap.get(col.getColumnType()) || 'start',
      });
    }
    return map;
  }, [table.relation]);

  const rowGetter = React.useCallback(
    (i) => formatRow(rows[i], columnsMap),
    [rows, columnsMap]);

  const onSort = (sortState: SortState) => {
    const column = columnsMap.get(sortState.dataKey);
    setRows(rows.sort(getSortFunc(sortState.dataKey, column.type, sortState.direction)));
  };

  if (rows.length === 0) {
    return null;
  }

  return (
    <DataTable
      rowGetter={rowGetter}
      rowCount={rows.length}
      columns={[...columnsMap.values()]}
      compact={true}
      onSort={onSort}
    />
  );
};
