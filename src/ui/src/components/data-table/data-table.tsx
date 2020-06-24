import clsx from 'clsx';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';

import {
    Column, SortDirection, SortDirectionType, Table, TableCellProps, TableCellRenderer,
    TableHeaderProps, TableHeaderRenderer,
} from 'react-virtualized';
import withAutoSizer, { WithAutoSizerProps } from 'utils/autosizer';
import noop from 'utils/noop';

import { createStyles, makeStyles, Theme, useTheme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';

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
    }
  }),
);

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
  onSort = noop,
  highlightedRow = -1,
}) => {
  if (width === 0 || height === 0) {
    return null;
  }

  const classes = useStyles();
  const theme = useTheme();
  const rowHeight = compact ? theme.spacing(4) : theme.spacing(6);

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
    }
    return ratio;
  }, [columns, rowGetter, rowCount]);

  const [widthOverrides, setColumnWidthOverride] = React.useState<ColWidthOverrides>({});

  const cellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => {
    if (props.columnData.cellRenderer) {
      return props.columnData.cellRenderer(props.cellData);
    }
    return <span className={classes.cellText}>{String(props.cellData)}</span>;
  }, []);

  const tableRef = React.useRef(null);

  const [sortState, setSortState] = React.useState<SortState>({ dataKey: '', direction: SortDirection.DESC });

  const rowGetterWrapper = React.useCallback(({ index }) => rowGetter(index), [rowGetter]);

  const onSortWrapper = React.useCallback(({ sortBy, sortDirection }) => {
    if (sortBy) {
      const nextSortState = { dataKey: sortBy, direction: sortDirection };
      setSortState(nextSortState);
      onSort(nextSortState);
      tableRef.current.forceUpdateGrid();
    }
  }, [onSort]);

  const onRowClickWrapper = React.useCallback(({ index }) => {
    onRowClick(index)
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
  return (
    <Table
      headerHeight={rowHeight}
      ref={tableRef}
      className={classes.table}
      overscanRowCount={2}
      rowGetter={rowGetterWrapper}
      rowCount={rowCount}
      rowHeight={rowHeight}
      onRowClick={onRowClickWrapper}
      rowClassName={getRowClass}
      height={height}
      width={width}
      sortDirection={sortState.direction}
      sortBy={sortState.dataKey}
    >
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
