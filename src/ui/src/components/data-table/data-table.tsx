import clsx from 'clsx';
import * as React from 'react';
import {
    CellMeasurer, CellMeasurerCache, ColumnSizer, GridCellRenderer, MultiGrid,
} from 'react-virtualized';
import withAutoSizer, {WithAutoSizerProps} from 'utils/autosizer';
import noop from 'utils/noop';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    table: {
      color: theme.palette.text.primary,
    },
    cell: {
      paddingLeft: theme.spacing(3),
      paddingRight: theme.spacing(3),
      backgroundColor: theme.palette.background.default,
      whiteSpace: 'nowrap',
      display: 'flex',
      alignItems: 'center',
      borderBottom: `solid 1px ${theme.palette.background.three}`,
      maxWidth: '100%',
    },
    compact: {
      paddingLeft: theme.spacing(2),
      paddingRight: theme.spacing(2),
    },
    header: {
      ...theme.typography.caption,
      borderTop: `solid 1px ${theme.palette.background.three}`,
      '&:last-of-type': {
        borderRight: `solid 1px ${theme.palette.background.three}`,
      },
      '&:first-of-type': {
        borderLeft: `solid 1px ${theme.palette.background.three}`,
      },
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

type SortDirection = 'asc' | 'desc';

export interface SortState {
  prevDataKey?: string;
  prevDirection?: SortDirection;
  dataKey: string;
  direction: SortDirection;
}

function sortReducer(state: SortState, dataKey: string): SortState {
  if (dataKey === state.dataKey) {
    return {
      prevDataKey: state.dataKey,
      prevDirection: state.direction,
      direction: state.direction === 'asc' ? 'desc' : 'asc',
      dataKey,
    };
  }
  return {
    prevDataKey: state.dataKey,
    prevDirection: state.direction,
    direction: 'desc',
    dataKey,
  };
}

interface HeaderProps extends ColumnProps {
  sort: SortDirection | '';
}

const Header = (props: HeaderProps) => {
  const classes = useStyles();
  let sortIcon = <div className={classes.sortPlaceholder} />;
  if (props.sort === 'asc') {
    sortIcon = <UpIcon className={classes.sortIcon} />;
  } else if (props.sort === 'desc') {
    sortIcon = <DownIcon className={classes.sortIcon} />;
  }
  if (props.align === 'end') {
    return <>
      {sortIcon}
      {props.label}
    </>;
  }
  return <>
    {props.label}
    {sortIcon}
  </>;
};

export const DataTable = withAutoSizer<DataTableProps>(React.memo<WithAutoSizerProps<DataTableProps>>(({
  columns,
  onRowClick,
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

  // Header row offset
  highlightedRow = highlightedRow + 1;

  const gridRef = React.useRef(null);
  const sizeCache = React.useMemo(() => new CellMeasurerCache({
    defaultWidth: 100,
    defaultHeight: rowHeight,
    fixedHeight: true,
  }), []);

  const [sortState, dispatchSortState] = React.useReducer(sortReducer, { dataKey: '', direction: 'desc' });
  React.useEffect(() => {
    if (sortState.dataKey) {
      onSort(sortState);
      gridRef.current.forceUpdateGrids();
    }
  }, [sortState]);

  React.useEffect(() => {
    if (highlightedRow !== 0) {
      gridRef.current.forceUpdateGrids();
    }
  }, [highlightedRow]);

  const cellRenderer: GridCellRenderer = React.useCallback(({
    columnIndex,
    rowIndex,
    key,
    parent,
    style,
  }) => {
    const column = columns[columnIndex];
    const isHeader = rowIndex === 0;
    let onClick = noop;
    if (isHeader) {
      onClick = () => {
        dispatchSortState(column.dataKey);
      };
    } else if (onRowClick) {
      onClick = () => onRowClick(rowIndex - 1);
    }

    const className = clsx(
      classes.cell,
      isHeader && classes.header,
      column.align && classes[column.align],
      compact && classes.compact,
      onClick !== noop && classes.clickable,
      !isHeader && rowIndex === highlightedRow && classes.highlighted,
      !isHeader && onClick !== noop && classes.highlightable,
    );
    const content = isHeader ?
      <Header {...column} sort={sortState.dataKey === column.dataKey ? sortState.direction : ''} /> :
      rowGetter(rowIndex - 1)[column.dataKey];

    return (
      <CellMeasurer
        cache={sizeCache}
        columnIndex={columnIndex}
        rowIndex={rowIndex}
        key={key}
        parent={parent}
      >
        {({ registerChild }) => (
          <div
            className={className}
            ref={registerChild}
            onClick={onClick}
            style={{ height: rowHeight, ...style }}
          >
            {content}
          </div>
        )}
      </CellMeasurer>
    );
  }, [sortState, highlightedRow]);

  if (width === 0 || height === 0) {
    return null;
  }

  return (
    <MultiGrid
      ref={gridRef}
      className={classes.table}
      columnCount={columns.length}
      columnWidth={sizeCache.columnWidth}
      deferredMeasurementCache={sizeCache}
      fixedColumnCount={0}
      fixedRowCount={1}
      height={height}
      overscanColumnCount={0}
      overscanRowCount={2}
      cellRenderer={cellRenderer}
      /* One extra row for the headers. */
      rowCount={rowCount + 1}
      rowHeight={rowHeight}
      width={width}
      sortBy={sortState.dataKey}
      sortDirection={sortState.direction}
    />
  );
}));
