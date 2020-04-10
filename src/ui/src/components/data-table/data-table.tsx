import clsx from 'clsx';
import * as React from 'react';
import {
    CellMeasurer, CellMeasurerCache, ColumnSizer, GridCellRenderer, MultiGrid,
} from 'react-virtualized';
import withAutoSizer, {WithAutoSizerProps} from 'utils/autosizer';
import noop from 'utils/noop';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    table: {
      color: theme.palette.text.primary,
    },
    cell: {
      paddingLeft: theme.spacing(3),
      paddingRight: theme.spacing(3),
      backgroundColor: theme.palette.background.two,
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
      ...theme.typography.subtitle2,
      fontWeight: theme.typography.fontWeightMedium,
      backgroundColor: theme.palette.background.three,
    },
    noClick: {
      cursor: 'initial',
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
  }),
);

export interface ColumnProps {
  dataKey: string;
  label: string;
  width?: number;
  align?: 'center' | 'start' | 'end';
}

interface DataTableProps {
  columns: ColumnProps[];
  onRowClick?: (rowIndex: number) => void;
  rowGetter: (rowIndex: number) => { [key: string]: React.ReactNode };
  rowCount: number;
  compact?: boolean;
}

export const DataTable = withAutoSizer<DataTableProps>(React.memo<WithAutoSizerProps<DataTableProps>>(({
  columns,
  onRowClick,
  rowCount,
  width,
  height,
  rowGetter,
  compact = false,
}) => {
  const classes = useStyles();
  const theme = useTheme();
  const rowHeight = compact ? theme.spacing(4) : theme.spacing(6);

  const sizeCache = React.useMemo(() => new CellMeasurerCache({
    defaultWidth: 100,
    defaultHeight: rowHeight,
    fixedHeight: true,
  }), []);

  React.useEffect(() => {
    sizeCache.clearAll();
  }, [width, height]);

  const cellRenderer: GridCellRenderer = React.useCallback(({
    columnIndex,
    rowIndex,
    key,
    parent,
    style,
  }) => {
    const column = columns[columnIndex];
    const isHeader = rowIndex === 0;
    const className = clsx(
      classes.cell,
      isHeader && classes.header,
      column.align && classes[column.align],
      compact && classes.compact,
    );
    const content = isHeader ? column.label : rowGetter(rowIndex - 1)[column.dataKey];
    const onClick = isHeader || !onRowClick ? noop : () => {
      onRowClick(rowIndex - 1);
    };
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
  }, []);

  return (
    <MultiGrid
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
    />
  );
}));
