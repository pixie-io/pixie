import clsx from 'clsx';
import * as React from 'react';
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
    sortPlaceholder: {
      paddingRight: theme.spacing(2),
    },
    sortIcon: {
      width: theme.spacing(2),
    },
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
  highlightedRow?: number;
}

export interface SortState {
  dataKey: string;
  direction: SortDirectionType;
}

export const DataTable = withAutoSizer<DataTableProps>(React.memo<WithAutoSizerProps<DataTableProps>>(({
  columns,
  onRowClick = noop,
  rowCount,
  width,
  height,
  rowGetter,
  compact = false,
  onSort = noop,
  highlightedRow = -1,
}) => {
  const classes = useStyles();
  const theme = useTheme();
  const rowHeight = compact ? theme.spacing(4) : theme.spacing(6);

  const headerRenderer: TableHeaderRenderer = React.useCallback((props: TableHeaderProps) => {
    let sortIcon = <div className={classes.sortPlaceholder} />;
    if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.ASC) {
      sortIcon = <UpIcon className={classes.sortIcon} />;
    } else if (props.sortBy === props.dataKey && props.sortDirection === SortDirection.DESC) {
      sortIcon = <DownIcon className={classes.sortIcon} />;
    }
    if (props.columnData.align === 'end') {
      return <>
        {sortIcon}
        <Tooltip title={props.label}>
          <span className={classes.cellText}>{props.label}</span>
        </Tooltip>
      </>;
    }
    return <>
      <Tooltip title={props.label}>
        <span className={classes.cellText}>{props.label}</span>
      </Tooltip>
      {sortIcon}
    </>;
  }, []);

  const cellRenderer: TableCellRenderer = React.useCallback((props: TableCellProps) => {
    if (props.columnData.cellRenderer) {
      return props.columnData.cellRenderer(props.cellData);
    }
    return <span className={classes.cellText}>{String(props.cellData)}</span>;
  }, []);

  const widthRatio = React.useMemo<number[]>(() => {
    // Randomly sample 10 rows to figure out the width basis of each row.
    const sampleCount = Math.min(10, rowCount);
    const ratio = columns.map((col) =>
      col.label.length + 2 /* sort icon space */
    );
    for (let i = 0; i < sampleCount; i++) {
      const rowIndex = Math.floor(Math.random() * Math.floor(rowCount));
      const row = rowGetter(rowIndex);
      columns.forEach((col, i) => {
        ratio[i] = Math.max(ratio[i], String(row[col.dataKey]).length);
      });
    }
    return ratio;
  }, [columns, rowGetter, rowCount]);

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

  if (width === 0 || height === 0) {
    return null;
  }

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
      sort={onSortWrapper}
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
            headerRenderer={headerRenderer}
            cellRenderer={cellRenderer}
            width={col.width || widthRatio[i]}
            flexGrow={1}
            flexShrink={1}
            columnData={col}
          />;
        })
      }
    </Table>
  );
}));
