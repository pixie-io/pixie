import { Table } from 'common/vizier-grpc-client';
import { DataTable, SortState } from 'components/data-table';
import * as React from 'react';
import { SortDirection, SortDirectionType } from 'react-virtualized';
import { DataType } from 'types/generated/vizier_pb';
import { DataAlignmentMap, GetDataSortFunc, JSONData } from 'utils/format-data';
import noop from 'utils/noop';
import { dataFromProto } from 'utils/result-data-utils';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';

function getSortFunc(dataKey: string, type: DataType, direction: SortDirectionType) {
  const f = GetDataSortFunc(type, direction === SortDirection.ASC);
  return (a, b) => f(a[dataKey], b[dataKey]);
}

interface VizierDataTableProps {
  table: Table;
  onRowSelectionChanged?: (row: any) => void;
}

export const VizierDataTable = (props: VizierDataTableProps) => {
  const { table, onRowSelectionChanged = noop } = props;
  const [rows, setRows] = React.useState([]);
  const [selectedRow, setSelectedRow] = React.useState(-1);

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
        align: DataAlignmentMap.get(col.getColumnType()) || 'start',
      });
    }
    return map;
  }, [table.relation]);

  const rowGetter = React.useCallback(
    (i) => rows[i],
    [rows, columnsMap]);

  const onSort = (sortState: SortState) => {
    const column = columnsMap.get(sortState.dataKey);
    setRows(rows.sort(getSortFunc(sortState.dataKey, column.type, sortState.direction)));
    setSelectedRow(-1);
    onRowSelectionChanged(null);
  };

  const onRowSelect = React.useCallback((rowIndex) => {
    if (rowIndex === selectedRow) {
      rowIndex = -1;
    }
    setSelectedRow(rowIndex);
    onRowSelectionChanged(rows[rowIndex]);
  }, [rows, selectedRow]);

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
      onRowClick={onRowSelect}
      highlightedRow={selectedRow}
    />
  );
};

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      display: 'flex',
      flexDirection: 'row',
      height: '100%',
      position: 'relative',
    },
    table: {
      flex: 3,
    },
    details: {
      flex: 1,
      padding: theme.spacing(2),
      borderLeft: `solid 1px ${theme.palette.background.three}`,
      minWidth: 0,
      overflow: 'auto',
    },
    close: {
      position: 'absolute',
    },
  }));

export const VizierDataTableWithDetails = (props: { table: Table }) => {
  const [details, setDetails] = React.useState(null);

  const classes = useStyles();
  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <VizierDataTable table={props.table} onRowSelectionChanged={(row) => { setDetails(row); }} />
      </div>
      <VizierDataRowDetails className={classes.details} data={details} />
    </div>
  );
};

interface VizierDataRowDetailsProps {
  data?: any;
  className?: string;
}

const VizierDataRowDetails = (props: VizierDataRowDetailsProps) => {
  const { data, className } = props;
  if (!data) {
    return null;
  }
  return <JSONData className={className} data={data} multiline={true} />;
};
