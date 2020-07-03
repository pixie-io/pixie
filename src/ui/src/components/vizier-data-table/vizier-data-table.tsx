import { Table } from 'common/vizier-grpc-client';
import { CellAlignment, DataTable, SortState } from 'components/data-table';
import * as React from 'react';
import { SortDirection, SortDirectionType } from 'react-virtualized';
import { DataType, Relation, SemanticType } from 'types/generated/vizier_pb';
import * as FormatData from 'utils/format-data';
import { getDataRenderer, getDataSortFunc } from 'utils/format-data';
import noop from 'utils/noop';
import { dataFromProto } from 'utils/result-data-utils';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import {
  EntityLink,
  isEntityType, STATUS_TYPES, toStatusIndicator,
} from '../live-widgets/utils';
import { AlertData, JSONData, LatencyData } from '../format-data/format-data';

function getSortFunc(dataKey: string, type: DataType, direction: SortDirectionType) {
  const f = getDataSortFunc(type, direction === SortDirection.ASC);
  return (a, b) => f(a[dataKey], b[dataKey]);
}

const DataAlignmentMap = new Map<DataType, CellAlignment>(
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

interface VizierDataTableProps {
  table: Table;
  prettyRender?: boolean;
  expandable?: boolean;
  expandedRenderer?: (rowIndex: number) => JSX.Element;
  // TODO(michelle/zasgar/nserrino): Remove this.
  clusterName?: string;
  onRowSelectionChanged?: (row: any) => void;
}

const statusRenderer = (st: SemanticType) => (v: string) => toStatusIndicator(v, st);

const serviceRendererFuncGen = (clusterName: string) => (v) => {
  try {
    // Hack to handle cases like "['pl/service1', 'pl/service2']" which show up for pods that are part of 2 services.
    const parsedArray = JSON.parse(v);
    if (Array.isArray(parsedArray)) {
      return (
        <>
          {
              parsedArray.map((entity, i) => (
                <span key={i}>
                  {i > 0 && ', '}
                  <EntityLink entity={entity} semanticType={SemanticType.ST_SERVICE_NAME} clusterName={clusterName} />
                </span>
              ))
            }
        </>
      );
    }
  } catch (e) {
    // noop.
  }
  return <EntityLink entity={v} semanticType={SemanticType.ST_SERVICE_NAME} clusterName={clusterName} />;
};

const entityRenderer = (st: SemanticType, clusterName: string) => {
  if (st === SemanticType.ST_SERVICE_NAME) {
    return serviceRendererFuncGen(clusterName);
  }
  const entity = (v) => (
    <EntityLink entity={v} semanticType={st} clusterName={clusterName} />
  );
  return entity;
};

const prettyCellRenderer = (colInfo: Relation.ColumnInfo, clusterName: string) => {
  const dt = colInfo.getColumnType();
  const st = colInfo.getColumnSemanticType();
  const name = colInfo.getColumnName();
  const renderer = getDataRenderer(dt);

  if (isEntityType(st)) {
    return entityRenderer(st, clusterName);
  }

  if (STATUS_TYPES.has(st)) {
    return statusRenderer(st);
  }

  if (FormatData.looksLikeLatencyCol(name, dt)) {
    return LatencyData;
  }

  if (FormatData.looksLikeAlertCol(name, dt)) {
    return AlertData;
  }

  if (dt !== DataType.STRING) {
    return renderer;
  }

  return (v) => {
    try {
      const jsonObj = JSON.parse(v);
      return (
        <JSONData
          data={jsonObj}
        />
      );
    } catch {
      return v;
    }
  };
};

export const VizierDataTable = (props: VizierDataTableProps) => {
  const {
    table, prettyRender = false, expandable = false, expandedRenderer,
    clusterName = null,
    onRowSelectionChanged = noop,
  } = props;
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
        cellRenderer: prettyRender ? prettyCellRenderer(col, clusterName) : getDataRenderer(col.getColumnType()),
      });
    }
    return map;
  }, [table.relation, clusterName, prettyRender]);

  const rowGetter = React.useCallback(
    (i) => rows[i],
    [rows],
  );

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
  }, [rows, selectedRow, onRowSelectionChanged]);

  if (rows.length === 0) {
    return null;
  }

  return (
    <DataTable
      rowGetter={rowGetter}
      rowCount={rows.length}
      columns={[...columnsMap.values()]}
      compact
      onSort={onSort}
      onRowClick={onRowSelect}
      highlightedRow={selectedRow}
      expandable={expandable}
      expandedRenderer={expandedRenderer}
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
        <VizierDataTable expandable={false} table={props.table} onRowSelectionChanged={(row) => { setDetails(row); }} />
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
  return <JSONData className={className} data={data} multiline />;
};
