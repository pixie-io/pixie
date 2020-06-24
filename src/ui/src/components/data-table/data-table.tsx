import clsx from 'clsx';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';

import {
    Column, SortDirection, SortDirectionType, Table, TableCellProps, TableCellRenderer,
    TableHeaderProps, TableHeaderRenderer, TableRowRenderer, TableRowProps, defaultTableRowRenderer
} from 'react-virtualized';
import withAutoSizer, { WithAutoSizerProps } from 'utils/autosizer';
import noop from 'utils/noop';

import { createStyles, makeStyles, Theme, useTheme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import * as expanded from 'images/icons/expanded.svg';
import * as unexpanded from 'images/icons/unexpanded.svg';

const EXPANDED_ROW_HEIGHT = 300;

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    table: {
      color: theme.palette.text.primary,
      '& > .ReactVirtualized__Table__headerRow': {
        ...theme.typography.caption,
        border: `solid 1px ${theme.palette.background.three}`,
        backgroundColor: theme.palette.background.default,
      },
    },
    row: {
      borderBottom: `solid 1px ${theme.palette.background.three}`,
      '& > .ReactVirtualized__Table__rowColumn:first-of-type' : {
        marginLeft: 0,
        marginRight: 0,
      },
      '&:hover $hidden': {
        display: 'flex',
      }
    },
    rowContainer: {
      borderBottom: `solid 1px ${theme.palette.background.three}`,
    },
    cell: {
      paddingLeft: theme.spacing(3),
      paddingRight: theme.spacing(3),
      backgroundColor: 'transparent',
      display: 'flex',
      alignItems: 'center',
      maxWidth: '33%',
      height: theme.spacing(6),
      margin: '0 !important',
    },
    cellText: {
      overflow: 'hidden',
      whiteSpace: 'nowrap',
      textOverflow: 'ellipsis',
    },
    compact: {
      paddingLeft: theme.spacing(1.5),
      paddingRight: 0,
      '&:last-of-type': {
        paddingRight: theme.spacing(1.5),
      },
      height: theme.spacing(4),
    },
    clickable: {
      cursor: 'pointer',
    },
    highlighted: {
      backgroundColor: theme.palette.background.three,
    },
    highlightable: {
      '&:hover': {
        backgroundColor: theme.palette.background.three,
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
    }
  }),
);

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
  expandedRenderer?: (rowIndex: number) => JSX.Element;
  highlightedRow?: number;
}

export interface SortState {
  dataKey: string;
  direction: SortDirectionType;
}

export interface ColWidthOverrides {
  [dataKey: string]: number;
}

export const DataTable = withAutoSizer<DataTableProps>(React.memo<WithAutoSizerProps<DataTableProps>>(({
  columns,
  onRowClick = noop,
  rowCount,
  width,
  height,
  rowGetter,
  compact = false,
  resizableColumns= false,
  expandable = true,
  expandedRenderer = () => <></>,
  onSort = noop,
  highlightedRow = -1,
}) => {
  if (width === 0 || height === 0) {
    return null;
  }

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
        // Try to compute the column width based on the col sizes.
        w = col.label.length + 2 /* sort icon space */;
        for (let i = 0; i < sampleCount; i++) {
          const rowIndex = Math.floor(Math.random() * Math.floor(rowCount));
          const row = rowGetter(rowIndex);
          w = Math.max(w, String(row[col.dataKey]).length);
        }
      }
      totalWidth += w;
      colsWidth[col.dataKey] = w;
    });

    const ratio: {[dataKey: string]: number} = {};
    for (const colsWidthKey in colsWidth) {
      ratio[colsWidthKey] = colsWidth[colsWidthKey] / totalWidth;
      // Enforce max-width.
      if (ratio[colsWidthKey] > 1/3) {
        ratio[colsWidthKey] = 1/3;
      }
    }
    return ratio;
  }, [columns, rowGetter, rowCount]);

  const [widthOverrides, setColumnWidthOverride] = React.useState<ColWidthOverrides>({});
  const tableRef = React.useRef(null);

  const [sortState, setSortState] = React.useState<SortState>({ dataKey: '', direction: SortDirection.DESC });
  const [expandedRowState, setExpandedRowstate] = React.useState<ExpandedRows>({});

  const rowGetterWrapper = React.useCallback(({ index }) => rowGetter(index), [rowGetter]);

  const cellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => {
    return <>
      {props.columnData.cellRenderer && props.columnData.cellRenderer(props.cellData)}
      {!props.columnData.cellRenderer && <span className={classes.cellText}>{String(props.cellData)}</span>}
    </>;
  }, [expandedRowState, expandedRenderer]);

  const defaultCellHeight = compact ? theme.spacing(4) : theme.spacing(6);
  const computeRowHeight = React.useCallback(({index}) => {
    return expandedRowState[index] ? EXPANDED_ROW_HEIGHT : defaultCellHeight;
  }, [defaultCellHeight, expandedRowState]);

  const onSortWrapper = React.useCallback(({ sortBy, sortDirection }) => {
    if (sortBy) {
      const nextSortState = { dataKey: sortBy, direction: sortDirection };
      setSortState(nextSortState);
      onSort(nextSortState);
      tableRef.current.forceUpdateGrid();
    }
  }, [onSort]);

  const onRowClickWrapper = React.useCallback(({ index }) => {
    setExpandedRowstate((state) => {
      const expandedRows = {...state};
      if (expandedRows[index]) {
        delete expandedRows[index];
      } else {
        expandedRows[index] = true;
      }
      return expandedRows;
    });
    tableRef.current.recomputeRowHeights();
    tableRef.current.forceUpdate();
    onRowClick(index);
  }, [onRowClick]);

  const getRowClass = React.useCallback(({ index }) => {
    if (index === -1) {
      return;
    }
    return clsx(
      classes.row,
      onRowClick && classes.clickable,
      onRowClick && classes.highlightable,
      index === highlightedRow && classes.highlighted
    );
  }, [highlightedRow]);

  const resizeColumn = React.useCallback(({dataKey, deltaX}) => {
    setColumnWidthOverride((state) => {
      const colIdx = columns.findIndex((col) => col.dataKey == dataKey);
      if (colIdx == -1) {
        return state;
      }

      const nextColKey = columns[colIdx+1].dataKey;
      let newWidth = state[dataKey] || (colTextWidthRatio[dataKey]);
      let nextColWidth = state[nextColKey] || (colTextWidthRatio[nextColKey]);

      const percentDelta = deltaX / width;
      newWidth += percentDelta;
      nextColWidth -= percentDelta;

      // Enforce max-width.
      if (newWidth > 1/3) {
        const d = newWidth - 1/3;
        newWidth = 1/3;
        nextColWidth += d;
      }

      return {
        ...state,
        [dataKey]: newWidth,
        [nextColKey]: nextColWidth,
      };
    });

  }, [width, colTextWidthRatio]);

  const colIsResizable = (idx: number): boolean => {
    return (resizableColumns||true) && (idx != columns.length - 1);
  };

  const headerRendererCommon: TableHeaderRenderer = (props) =>{
    let sortIcon = <UpIcon className={classes.sortIconHidden} onClick={() => {
        onSortWrapper({sortBy: props.dataKey, sortDirection: SortDirection.ASC});
    }}/>;
    if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.ASC) {
      sortIcon = <UpIcon className={classes.sortIcon} onClick={() => {
        onSortWrapper({sortBy: props.dataKey, sortDirection: SortDirection.DESC});
      }}/>;
    } else if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.DESC) {
      sortIcon = <DownIcon className={classes.sortIcon} onClick={() => {
        onSortWrapper({sortBy: props.dataKey, sortDirection: SortDirection.ASC});
      }}/>;
    }
    const headerStyle = clsx(
      [classes.headerTitle],
      [classes[props.columnData.align]],
    );

    return <>
      <div className={headerStyle}>
        <Tooltip title={props.label}>
          <span className={classes.cellText}>{props.label}</span>
        </Tooltip>
        {sortIcon}
      </div>
    </>;
  };

  const headerRenderer: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => {
    return <>
    <React.Fragment key={props.dataKey}>
      {headerRendererCommon(props)}
    </React.Fragment>
    </>;
  }, []);

  const gutterHeaderRenderer: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => {
    return <>
      <React.Fragment key={props.dataKey}>
      </React.Fragment>
    </>;
  }, []);

  const gutterCellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => {
    // Hide the icon by default unless:
    //  1. It's been expanded.
    //  2. The row has been highlighted.
    const cls = clsx(
      classes.gutterCell,
      !(highlightedRow == props.rowIndex || expandedRowState[props.rowIndex]) && classes.hidden,
    )
    const icon = expandedRowState[props.rowIndex] ? expanded : unexpanded;
    return <>
      <div className={cls}>
        <img src={icon} />
      </div>
    </>;
  }, [highlightedRow, expandedRowState]);

  const rowRenderer: TableRowRenderer = React.useCallback((props: TableRowProps) => {
    return <div
        className={classes.rowContainer}
        key={props.key}
        style={props.style}
      >
      {defaultTableRowRenderer({ ...props, key: '', style: {height: defaultCellHeight} })}

      {expandedRowState[props.index] &&
         <div className={classes.expandedCell}>
           {expandedRenderer(props.index)}
         </div>
      }
    </div>;
  }, [expandedRowState]);

  const headerRendererWithDrag: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => {
    const dataKey = props.dataKey;
    return <>
      <React.Fragment key={dataKey}>
        {headerRendererCommon(props)}
        <DraggableCore
            onDrag={(event, { deltaX }) => {
              resizeColumn({
                dataKey,
                deltaX,
              });
            }}>
          <span className={classes.dragHandle}>&#8942;</span>
        </DraggableCore>
      </React.Fragment>
    </>;
  }, []);

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
    >
      {
        expandable &&
        <Column
          key='gutter'
          dataKey={'gutter'}
          label={''}
          headerClassName={gutterClass}
          className={gutterClass}
          headerRenderer={gutterHeaderRenderer}
          cellRenderer={gutterCellRenderer}
          width={4 /*width for chevron */}
          columnData={null}
        />
      }
      {
        columns.map((col, i) => {
          const className = clsx(
            classes.cell,
            classes[col.align],
            compact && classes.compact,
          );
          return <Column
            key={col.dataKey}
            dataKey={col.dataKey}
            label={col.label}
            headerClassName={className}
            className={className}
            headerRenderer={colIsResizable(i) ? headerRendererWithDrag : headerRenderer}
            cellRenderer={cellRenderer}
            width={(widthOverrides[col.dataKey] || colTextWidthRatio[col.dataKey]) * width}
            columnData={col}
          />;
        })
      }
    </Table>
  );
}));
