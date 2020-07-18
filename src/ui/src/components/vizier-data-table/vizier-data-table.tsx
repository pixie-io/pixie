import { Table } from 'common/vizier-grpc-client';
import {
  CellAlignment, ColumnProps, DataTable, SortState,
} from 'components/data-table';
import { JSONData } from 'components/format-data/format-data';
import * as React from 'react';
import { DataType, Relation, SemanticType } from 'types/generated/vizier_pb';
import noop from 'utils/noop';
import { dataFromProto } from 'utils/result-data-utils';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import { IndexRange } from 'react-virtualized';
import { ColumnDisplayInfo, displayInfoFromColumn, titleFromInfo } from './column-display-info';
import { parseRows } from './parsers';
import { vizierCellRenderer } from './renderers';
import { getSortFunc } from './sort-funcs';

const DataAlignmentMap = new Map<DataType, CellAlignment>(
  [
    [DataType.BOOLEAN, 'center'],
    [DataType.INT64, 'end'],
    [DataType.UINT128, 'start'],
    [DataType.FLOAT64, 'end'],
    [DataType.STRING, 'start'],
    [DataType.TIME64NS, 'end'],
  ],
);

// For certain semantic types, override the column width ratio based off of the rendering
// we expect to do for that semantic type.
const SemanticTypeWidthOverrideMap = new Map<SemanticType, number>(
  [
    [SemanticType.ST_QUANTILES, 40],
  ],
);
const DataTypeWidthOverrideMap = new Map<DataType, number>(
  [
    [DataType.TIME64NS, 25],
  ],
);

function hasWidthOverride(st: SemanticType, dt: DataType): boolean {
  return SemanticTypeWidthOverrideMap.has(st) || DataTypeWidthOverrideMap.has(dt);
}

function getWidthOverride(st: SemanticType, dt: DataType): number {
  if (SemanticTypeWidthOverrideMap.has(st)) {
    return SemanticTypeWidthOverrideMap.get(st);
  }
  return DataTypeWidthOverrideMap.get(dt);
}

interface VizierDataTableProps {
  table: Table;
  prettyRender?: boolean;
  expandable?: boolean;
  expandedRenderer?: (rowIndex: number) => JSX.Element;
  // TODO(michelle/zasgar/nserrino): Remove this.
  clusterName?: string;
  onRowSelectionChanged?: (row: any) => void;
  onRowsRendered?: (range: IndexRange) => void;
}

export const VizierDataTable = (props: VizierDataTableProps) => {
  const {
    table, prettyRender = false, expandable = false, expandedRenderer,
    clusterName = null,
    onRowSelectionChanged = noop,
    onRowsRendered = () => {},
  } = props;
  const [rows, setRows] = React.useState([]);
  const [selectedRow, setSelectedRow] = React.useState(-1);
  const [columnDisplayInfos, setColumnDisplayInfos] = React.useState<Map<string, ColumnDisplayInfo>>(
    new Map<string, ColumnDisplayInfo>());

  React.useEffect(() => {
    // Map containing the display information for the column.
    const displayInfos = new Map<string, ColumnDisplayInfo>();

    table.relation.getColumnsList().forEach((col) => {
      const name = col.getColumnName();
      const displayInfo = displayInfoFromColumn(col);
      displayInfos.set(name, displayInfo);
    });

    const semanticTypeMap = [...displayInfos.values()].reduce((acc, val) => {
      acc.set(val.columnName, val.semanticType);
      return acc;
    }, new Map<string, SemanticType>());

    const rawRows = dataFromProto(table.relation, table.data);
    if (prettyRender) {
      const parsedRows = parseRows(semanticTypeMap, rawRows);
      setRows(parsedRows);
    } else {
      setRows(rawRows);
    }
    setColumnDisplayInfos(displayInfos);
  }, [table.relation, table.data, clusterName, prettyRender]);

  const dataTableCols = React.useMemo((): ColumnProps[] => (
    [...columnDisplayInfos.values()].map((displayInfo: ColumnDisplayInfo) => {
      // Some cells give the power to update the display state for the whole column.
      // This function is the handle that allows them to do that.
      const updateColumnDisplay = ((newColumnDisplay: ColumnDisplayInfo) => {
        const newMap = new Map<string, ColumnDisplayInfo>(
          columnDisplayInfos.set(displayInfo.columnName, newColumnDisplay));
        setColumnDisplayInfos(newMap);
      });

      const colProps: ColumnProps = {
        dataKey: displayInfo.columnName,
        label: titleFromInfo(displayInfo),
        align: DataAlignmentMap.get(displayInfo.type) || 'start',
        cellRenderer: vizierCellRenderer(displayInfo, updateColumnDisplay, prettyRender, clusterName, rows),
      };
      if (hasWidthOverride(displayInfo.semanticType, displayInfo.type)) {
        colProps.width = getWidthOverride(displayInfo.semanticType, displayInfo.type);
      }
      return colProps;
    })
  ), [rows, columnDisplayInfos, clusterName, prettyRender]);

  const rowGetter = React.useCallback(
    (i) => rows[i],
    [rows],
  );

  const onSort = (sortState: SortState) => {
    const column = columnDisplayInfos.get(sortState.dataKey);
    setRows(rows.sort(getSortFunc(column, sortState.direction)));
    setSelectedRow(-1);
    onRowSelectionChanged(null);
  };

  const onRowSelect = React.useCallback((rowIndex) => {
    if (rowIndex === selectedRow) {
      rowIndex = -1;
    }
    setSelectedRow(rowIndex);
    onRowSelectionChanged(rows[rowIndex]);
  }, [rows, selectedRow, onRowSelectionChanged]);

  if (rows.length === 0) {
    return null;
  }

  return (
    <DataTable
      rowGetter={rowGetter}
      rowCount={rows.length}
      columns={dataTableCols}
      compact
      onSort={onSort}
      onRowClick={onRowSelect}
      highlightedRow={selectedRow}
      expandable={expandable}
      expandedRenderer={expandedRenderer}
      onRowsRendered={onRowsRendered}
    />
  );
};

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    flexDirection: 'row',
    height: '100%',
    position: 'relative',
  },
  details: {
    flex: 1,
    padding: theme.spacing(2),
    borderLeft: `solid 1px ${theme.palette.background.three}`,
    minWidth: 0,
    overflow: 'auto',
    whiteSpace: 'pre-wrap',
  },
  table: {
    flex: 3,
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
        <VizierDataTable expandable={false} table={props.table} onRowSelectionChanged={(row) => { setDetails(row); }} />
      </div>
      <VizierDataRowDetails data={details} />
    </div>
  );
};

interface VizierDataRowDetailsProps {
  data?: any;
}

const VizierDataRowDetails = ({ data }: VizierDataRowDetailsProps) => {
  const classes = useStyles();
  if (!data) {
    return null;
  }
  return (
    <div className={classes.details}>
      <JSONData data={data} multiline />
    </div>
  );
};
