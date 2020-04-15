import {Table} from 'common/vizier-grpc-client';
import {CellAlignment, ColumnProps, DataTable} from 'components/data-table';
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

export const VizierDataTable = (props: VizierDataTableProps) => {
  const { table } = props;
  const rows = React.useMemo(() =>
    dataFromProto(table.relation, table.data, getDataRenderer),
    [table.relation, table.data]);
  const columns: ColumnProps[] = table.relation.getColumnsList().map((col) => {
    return {
      dataKey: col.getColumnName(),
      label: col.getColumnName(),
      align: AlignmentMap.get(col.getColumnType()) || 'start',
    };
  });
  return (
    <DataTable
      rowGetter={(i) => rows[i]}
      rowCount={rows.length}
      columns={columns}
      compact={true}
    />
  );
};
