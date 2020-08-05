import clsx from 'clsx';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';

import {
  Column, SortDirection, SortDirectionType, Table, TableCellProps, TableCellRenderer,
  TableHeaderProps, TableHeaderRenderer, TableRowRenderer, TableRowProps, defaultTableRowRenderer, IndexRange,
} from 'react-virtualized';
import withAutoSizer, { WithAutoSizerProps } from 'utils/autosizer';
import noop from 'utils/noop';

import {
  createStyles, makeStyles, Theme, useTheme,
} from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import * as expanded from 'images/icons/expanded.svg';
import * as unexpanded from 'images/icons/unexpanded.svg';
import * as seedrandom from 'seedrandom';

const EXPANDED_ROW_HEIGHT = 300;
// The maximum number of characters to use for each column in determining sizing.
// This prevents cases where one really large column dominates the entire table.
const MAX_COL_CHAR_WIDTH = 50;

const useStyles = makeStyles((theme: Theme) => createStyles({
  table: {
    color: theme.palette.text.primary,
    '& > .ReactVirtualized__Table__headerRow': {
      ...theme.typography.caption,
      border: `solid 1px ${theme.palette.foreground.grey3}`,
      backgroundColor: theme.palette.background.default,
      display: 'flex',
    },
  },
  row: {
    borderBottom: `solid 1px ${theme.palette.foreground.grey3}`,
    '& > .ReactVirtualized__Table__rowColumn:first-of-type': {
      marginLeft: 0,
      marginRight: 0,
    },
    '&:hover $hidden': {
      display: 'flex',
    },
    display: 'flex',
    fontSize: '0.875rem',
  },
  rowContainer: {
    borderBottom: `solid 1px ${theme.palette.foreground.grey3}`,
    display: 'flex',
    flexDirection: 'column',
    paddingRight: '0 !important',
  },
  cell: {
    paddingLeft: theme.spacing(3),
    paddingRight: theme.spacing(3),
    backgroundColor: 'transparent',
    display: 'flex',
    alignItems: 'center',
    height: theme.spacing(6),
    margin: '0 !important',
    whiteSpace: 'nowrap',
    overflow: 'hidden',
  },
  cellText: {
    overflow: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
  },
  compact: {
    paddingLeft: theme.spacing(1.5),
    paddingRight: 0,
    height: theme.spacing(4),
  },
  clickable: {
    cursor: 'pointer',
  },
  highlighted: {
    backgroundColor: theme.palette.foreground.grey3,
  },
  highlightable: {
    '&:hover': {
      backgroundColor: theme.palette.foreground.grey3,
    },
  },
  center: {
    justifyContent: 'center',
  },
  start: {
    justifyContent: 'flex-start',
  },
  end: {
    justifyContent: 'flex-end',
  },
  sortIcon: {
    width: theme.spacing(2),
    paddingLeft: theme.spacing(1),
  },
  sortIconHidden: {
    width: theme.spacing(2),
    opacity: '0.2',
    paddingLeft: theme.spacing(1),
  },
  headerTitle: {
    display: 'flex',
    alignItems: 'center',
    flex: 'auto',
    overflow: 'hidden',
  },
  gutterCell: {
    paddingLeft: '0px',
    flex: 'auto',
    alignItems: 'center',
    // TODO(michelle/zasgar): Fix this.
    overflow: 'visible',
    minWidth: theme.spacing(2.5),
    display: 'flex',
    height: '100%',
  },
  dragHandle: {
    flex: '0 0 12px',
    display: 'flex',
    flexDirection: 'column',
    justifyContent: 'center',
    alignItems: 'center',
    cursor: 'col-resize',
    '&:hover': {
      color: theme.palette.foreground.white,
    },
  },
  expandedCell: {
    overflow: 'auto',
    flex: 1,
    paddingLeft: '20px',
  },
  hidden: {
    display: 'none',
  },
  cellWrapper: {
    paddingRight: theme.spacing(1.5),
    width: '100%',
    display: 'flex',
  },
  innerCell: {
    overflow: 'hidden',
    textOverflow: 'ellipsis',
  },
}));

export interface ExpandedRows {
  [key: number]: boolean;
}

export type CellAlignment = 'center' | 'start' | 'end';

export interface ColumnProps {
  dataKey: string;
  label: string;
  width?: number;
  align?: CellAlignment;
  cellRenderer?: (data: any) => React.ReactNode;
}

interface DataTableProps {
  columns: ColumnProps[];
  onRowClick?: (rowIndex: number) => void;
  rowGetter: (rowIndex: number) => { [key: string]: React.ReactNode };
  onSort?: (sort: SortState) => void;
  rowCount: number;
  compact?: boolean;
  resizableColumns?: boolean;
  expandable?: boolean;
  expandedRenderer?: (rowData: any) => JSX.Element;
  highlightedRow?: number;
  onRowsRendered?: (range: IndexRange) => void;
}

export interface SortState {
  dataKey: string;
  direction: SortDirectionType;
}

export interface ColWidthOverrides {
  [dataKey: string]: number;
}

// eslint-disable-next-line react/display-name
const DataTable = withAutoSizer<DataTableProps>(React.memo<WithAutoSizerProps<DataTableProps>>((props) => {
  const { width, height } = props;
  if (width === 0 || height === 0) {
    return null;
  }
  return <InternalDataTable {...props} />;
}));

(DataTable as React.SFC).displayName = 'DataTable';

export { DataTable };

const InternalDataTable = ({
  columns,
  onRowClick = noop,
  rowCount,
  width,
  height,
  rowGetter,
  compact = false,
  resizableColumns = false,
  expandable = false,
  expandedRenderer = () => <></>,
  onSort = noop,
  highlightedRow = -1,
  onRowsRendered = () => {},
}: WithAutoSizerProps<DataTableProps>) => {
  const classes = useStyles();
  const theme = useTheme();

  const colTextWidthRatio = React.useMemo<{[dataKey: string]: number}>(() => {
    const colsWidth: {[dataKey: string]: number} = {};
    // Randomly sample 10 rows to figure out the width basis of each row.
    const sampleCount = Math.min(10, rowCount);
    let totalWidth = 0;
    columns.forEach((col) => {
      let w = col.width || null;
      if (!w) {
        const rng = seedrandom(1234);
        for (let i = 0; i < sampleCount; i++) {
          const rowIndex = Math.floor(rng() * Math.floor(rowCount));
          const row = rowGetter(rowIndex);
          w = Math.min(Math.max(w, String(row[col.dataKey]).length), MAX_COL_CHAR_WIDTH);
        }
      }

      // We add 2 to the header width to accommodate type/sort icons.
      const headerWidth = col.label.length + 2;
      colsWidth[col.dataKey] = Math.min(Math.max(headerWidth, w), MAX_COL_CHAR_WIDTH);
      totalWidth += colsWidth[col.dataKey];
    });

    const ratio: {[dataKey: string]: number} = {};
    Object.keys(colsWidth).forEach((colsWidthKey) => {
      ratio[colsWidthKey] = colsWidth[colsWidthKey] / totalWidth;
    });
    return ratio;
  }, [columns, rowGetter, rowCount]);

  const [widthOverrides, setColumnWidthOverride] = React.useState<ColWidthOverrides>({});
  const tableRef = React.useRef(null);

  const [sortState, setSortState] = React.useState<SortState>({ dataKey: '', direction: SortDirection.DESC });
  const [expandedRowState, setExpandedRowstate] = React.useState<ExpandedRows>({});

  const rowGetterWrapper = React.useCallback(({ index }) => rowGetter(index), [rowGetter]);

  const cellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => (
    <div className={clsx(classes.cellWrapper, classes[props.columnData.align])}>
      <div className={classes.innerCell}>
        {props.columnData.cellRenderer && props.columnData.cellRenderer(props.cellData)}
        {!props.columnData.cellRenderer && <span className={classes.cellText}>{String(props.cellData)}</span>}
      </div>
    </div>
  ), [classes.cellText]);

  const defaultCellHeight = compact ? theme.spacing(4) : theme.spacing(6);
  const computeRowHeight = React.useCallback(({ index }) => (expandedRowState[index]
    ? EXPANDED_ROW_HEIGHT
    : defaultCellHeight), [defaultCellHeight, expandedRowState]);

  const onSortWrapper = React.useCallback(({ sortBy, sortDirection }) => {
    if (sortBy) {
      const nextSortState = { dataKey: sortBy, direction: sortDirection };
      setSortState(nextSortState);
      onSort(nextSortState);
      tableRef.current.forceUpdateGrid();
    }
  }, [onSort]);

  React.useEffect(() => {
    let sortKey;
    for (let i = 0; i < columns.length; ++i) {
      // Don't use an empty label which may be a gutter column.
      if (columns[i].label) {
        sortKey = columns[i].dataKey;
        break;
      }
    }
    if (sortKey && !sortState.dataKey) {
      onSortWrapper({ sortBy: sortKey, sortDirection: SortDirection.ASC });
    }
  }, [columns, onSortWrapper]);

  const onRowClickWrapper = React.useCallback(({ index }) => {
    if (expandable) {
      setExpandedRowstate((state) => {
        const expandedRows = { ...state };
        if (expandedRows[index]) {
          delete expandedRows[index];
        } else {
          expandedRows[index] = true;
        }
        return expandedRows;
      });
    }
    tableRef.current.recomputeRowHeights();
    tableRef.current.forceUpdate();
    onRowClick(index);
  }, [onRowClick, expandable]);

  const getRowClass = React.useCallback(({ index }) => {
    if (index === -1) {
      return null;
    }
    return clsx(
      classes.row,
      onRowClick && classes.clickable,
      onRowClick && classes.highlightable,
      index === highlightedRow && classes.highlighted,
    );
  }, [highlightedRow, onRowClick, classes]);

  const resizeColumn = React.useCallback(({ dataKey, deltaX }) => {
    setColumnWidthOverride((state) => {
      const colIdx = columns.findIndex((col) => col.dataKey === dataKey);
      if (colIdx === -1) {
        return state;
      }

      const nextColKey = columns[colIdx + 1].dataKey;
      let newWidth = state[dataKey] || (colTextWidthRatio[dataKey]);
      let nextColWidth = state[nextColKey] || (colTextWidthRatio[nextColKey]);

      const percentDelta = deltaX / width;

      newWidth += percentDelta;
      nextColWidth -= percentDelta;

      return {
        ...state,
        [dataKey]: newWidth,
        [nextColKey]: nextColWidth,
      };
    });
  }, [width, colTextWidthRatio, columns]);

  const colIsResizable = (idx: number): boolean => (resizableColumns || true) && (idx !== columns.length - 1);

  const headerRendererCommon: TableHeaderRenderer = React.useCallback((props) => {
    let sortIcon = (
      <UpIcon
        className={classes.sortIconHidden}
        onClick={() => {
          onSortWrapper({ sortBy: props.dataKey, sortDirection: SortDirection.ASC });
        }}
      />
    );
    if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.ASC) {
      sortIcon = (
        <UpIcon
          className={classes.sortIcon}
          onClick={() => {
            onSortWrapper({ sortBy: props.dataKey, sortDirection: SortDirection.DESC });
          }}
        />
      );
    } else if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.DESC) {
      sortIcon = (
        <DownIcon
          className={classes.sortIcon}
          onClick={() => {
            onSortWrapper({ sortBy: props.dataKey, sortDirection: SortDirection.ASC });
          }}
        />
      );
    }
    const headerStyle = clsx(
      [classes.headerTitle],
      [classes[props.columnData.align]],
    );

    return (
      <>
        <div className={headerStyle}>
          <Tooltip title={props.label}>
            <span className={classes.cellText}>{props.label}</span>
          </Tooltip>
          {sortIcon}
        </div>
      </>
    );
  }, [classes, onSortWrapper]);

  const headerRenderer: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => (
    <>
      <React.Fragment key={props.dataKey}>
        {headerRendererCommon(props)}
      </React.Fragment>
    </>
  ), [headerRendererCommon]);

  const gutterHeaderRenderer: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => (
    <>
      <React.Fragment key={props.dataKey} />
    </>
  ), []);

  const gutterCellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => {
    // Hide the icon by default unless:
    //  1. It's been expanded.
    //  2. The row has been highlighted.
    const cls = clsx(
      classes.gutterCell,
      !(highlightedRow === props.rowIndex || expandedRowState[props.rowIndex]) && classes.hidden,
    );
    const icon = expandedRowState[props.rowIndex] ? expanded : unexpanded;
    return (
      <>
        <div className={cls}>
          <img src={icon} />
        </div>
      </>
    );
  }, [highlightedRow, expandedRowState, classes]);

  const rowRenderer: TableRowRenderer = React.useCallback((props: TableRowProps) => {
    const { style } = props;
    style.width = '100%';
    return (
      <div
        className={classes.rowContainer}
        key={props.key}
        style={style}
      >
        {defaultTableRowRenderer({ ...props, key: '', style: { height: defaultCellHeight } })}

        {expandable && expandedRowState[props.index]
         && (
         <div className={classes.expandedCell}>
           {expandedRenderer(rowGetter(props.index))}
         </div>
         )}
      </div>
    );
  }, [classes, defaultCellHeight, expandedRowState, expandedRenderer, expandable, rowGetter]);

  const headerRendererWithDrag: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => {
    const { dataKey } = props;
    return (
      <>
        <React.Fragment key={dataKey}>
          {headerRendererCommon(props)}
          <DraggableCore
            onDrag={(event, { deltaX }) => {
              resizeColumn({
                dataKey,
                deltaX,
              });
            }}
          >
            <span className={classes.dragHandle}>&#8942;</span>
          </DraggableCore>
        </React.Fragment>
      </>
    );
  }, [classes, headerRendererCommon, resizeColumn]);
  const gutterClass = clsx(
    compact && classes.compact,
    classes.gutterCell,
  );
  return (
    <Table
      headerHeight={defaultCellHeight}
      ref={tableRef}
      className={classes.table}
      overscanRowCount={2}
      rowGetter={rowGetterWrapper}
      rowCount={rowCount}
      rowHeight={computeRowHeight}
      onRowClick={onRowClickWrapper}
      rowClassName={getRowClass}
      rowRenderer={rowRenderer}
      height={height}
      width={width}
      sortDirection={sortState.direction}
      sortBy={sortState.dataKey}
      onRowsRendered={onRowsRendered}
    >
      {
        expandable
        && (
        <Column
          key='gutter'
          dataKey='gutter'
          label=''
          headerClassName={gutterClass}
          className={gutterClass}
          headerRenderer={gutterHeaderRenderer}
          cellRenderer={gutterCellRenderer}
          width={4 /* width for chevron */}
          columnData={null}
        />
        )
      }
      {
        columns.map((col, i) => {
          const className = clsx(
            classes.cell,
            classes[col.align],
            compact && classes.compact,
          );
          return (
            <Column
              key={col.dataKey}
              dataKey={col.dataKey}
              label={col.label}
              headerClassName={className}
              className={className}
              headerRenderer={colIsResizable(i) ? headerRendererWithDrag : headerRenderer}
              cellRenderer={cellRenderer}
              width={(widthOverrides[col.dataKey] || colTextWidthRatio[col.dataKey]) * width}
              columnData={col}
            />
          );
        })
      }
    </Table>
  );
};
