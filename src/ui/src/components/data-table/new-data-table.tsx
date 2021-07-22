/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';
import {
  useTable,
  useBlockLayout,
  useResizeColumns,
  useSortBy,
  Column,
  Cell,
  HeaderGroup,
} from 'react-table';
import { FixedSizeList as List, areEqual } from 'react-window';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import { useScrollbarSize } from 'app/utils/use-scrollbar-size';
import { buildClass } from 'app/utils/build-class';

// Augments `@types/react-table` to recognize the plugins we're using
import './react-table-config.d';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';
import { alpha, Paper } from '@material-ui/core';

export interface ReactTable<D extends Record<string, any> = Record<string, any>> {
  columns: Array<Column<D>>;
  data: D[];
}

const ROW_HEIGHT_PX = 40;

const useDataTableStyles = makeStyles((theme: Theme) => createStyles({
  // Note: most display and overflow properties here are to align interactions between react-window and react-table.
  table: {
    display: 'block',
    fontSize: theme.typography.pxToRem(15),
    overflow: 'hidden',
  },
  tableHead: {
    position: 'sticky',
    top: 0,
    overflowY: 'auto',
    overflowX: 'hidden',
    minWidth: '100%',
    height: `${ROW_HEIGHT_PX}px`,
    // Make sure the body is behind, not in front of, the header on the Z axis.
    // We're using a <Paper /> for the color, and need to override some of its props to match perfectly.
    zIndex: 1,
    boxShadow: 'none',
    borderRadius: 0,
  },
  headerRow: {
    // paddingBottom: theme.spacing(1),
  },
  headerCell: {
    position: 'relative', // In case anything inside positions absolutely
    fontSize: theme.typography.pxToRem(14),
    padding: theme.spacing(1),
    alignSelf: 'baseline',
    '&:not(:last-child)': {
      // Makes sure everything still lines up
      borderRight: '1px solid transparent',
    },
  },
  headerCellContents: {
    display: 'flex',
    flexFlow: 'row nowrap',
    alignItems: 'center',
  },
  // Separate so that resize handles can clip visibly to the side (centering themselves on the border)
  headerLabel: {
    textTransform: 'uppercase',
    overflowX: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    fontWeight: 500,
    display: 'inline-block',
    maxWidth: '100%',
    flexGrow: 1,
  },
  headerLabelRight: {
    textAlign: 'right',
  },
  // The body is pushed down by the height of the (sticky-positioned) header. react-window already accounts for this.
  tableBody: {
    position: 'absolute',
  },
  bodyRow: {
    '&:not(:last-child)': {
      borderBottom: `1px solid ${theme.palette.background.three}`,
    },
  },
  bodyRowSelectable: {
    cursor: 'pointer',
    '&:hover': {
      backgroundColor: `${alpha(theme.palette.foreground.grey2, 0.42)}`,
    },
  },
  bodyRowSelected: {
    backgroundColor: theme.palette.foreground.grey3,
  },
  bodyCell: {
    position: 'relative', // In case anything inside positions absolutely
    display: 'flex',
    alignItems: 'center',
    padding: `0 ${theme.spacing(1)}`,
    height: `${ROW_HEIGHT_PX}px`, // Ensures the border stretches. See cellContents for the rest.
    '&:not(:last-of-type)': {
      borderRight: `1px solid ${theme.palette.background.three}`,
    },
  },
  cellContents: {
    display: 'inline-block',
    overflow: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    maxHeight: '100%',
    width: '100%',
    lineHeight: `${ROW_HEIGHT_PX}px`,
  },
  start: { textAlign: 'left' },
  end: { textAlign: 'right' },
  center: { textAlign: 'center' },
  fill: { textAlign: 'center' },
  sortButton: {
    opacity: 0.2,
    width: theme.spacing(3),
    paddingLeft: theme.spacing(1),
  },
  sortButtonActive: {
    opacity: 1,
  },
  resizeHandle: {
    userSelect: 'none',
    position: 'absolute',
    right: '0',
    top: '50%',
    transform: 'translate(50%, -50%)',
  },
  resizeHandleActive: {
    color: theme.palette.foreground.three,
    fontWeight: 'bold',
  },
}), { name: 'DataTable' });

const ColumnSortButton: React.FC<{ column: HeaderGroup }> = ({ column }) => {
  const classes = useDataTableStyles();
  const className = buildClass(
    classes.sortButton,
    column.isSorted && classes.sortButtonActive,
  );
  return column.isSortedDesc ? <DownIcon className={className} /> : <UpIcon className={className} />;
};

const ColumnResizeHandle: React.FC<{ column: HeaderGroup }> = ({ column }) => {
  const classes = useDataTableStyles();
  return (
    // TODO(nick,PC-1050): Add double-click reset all columns; make that reflow as well. Remember weights!
    <span
      {...column.getResizerProps()}
      className={buildClass(classes.resizeHandle, column.isResizing && classes.resizeHandleActive)}
    >
      &#8942;
    </span>
  );
};

export interface DataTableProps {
  table: ReactTable;
  /**
   * If the table's parent is elevated (via MaterialUI), the table needs to match it colors to colorize properly.
   * This is used to set a background in the sticky header, so that the scrolling table body doesn't appear behind it.
   */
  elevation?: number;
  enableRowSelect?: boolean;
  onRowSelected?: (row: Record<string, any>|null) => void;
}

const DataTableImpl: React.FC<DataTableProps> = ({ table, ...options }) => {
  const classes = useDataTableStyles();

  const { columns, data } = table;

  // Begin: width math
  const [scrollbarContainer, setScrollbarContainer] = React.useState<HTMLElement>(null);
  const scrollContainerRef = React.useCallback((el) => setScrollbarContainer(el), []);
  const { width: scrollbarWidth } = useScrollbarSize(scrollbarContainer);

  // TODO(nick,PC-1050): When this changes, need to reflow current widths (try to keep ratios). Only if not resized?
  const { width: containerWidth, height: containerHeight } = React.useContext(AutoSizerContext);

  // Once the header is rendered, use its height to push the topmost scroll position below said header.
  const [headingsHeight, setHeadingsHeight] = React.useState(0);
  const headingsRef = React.useCallback((el) => setHeadingsHeight(el?.offsetHeight ?? 0), []);

  // Space not claimed by fixed-width columns is evenly distributed among remaining columns.
  const defaultWidth = React.useMemo(() => {
    const staticWidths = columns.map((c) => Number(c.width)).filter((c) => c > 0);
    const staticSum = staticWidths.reduce((a, c) => a + c, 0);
    return (containerWidth - staticSum - scrollbarWidth) / (columns.length - staticWidths.length);
  }, [columns, containerWidth, scrollbarWidth]);

  const defaultColumn = React.useMemo(() => ({
    minWidth: 90,
    width: defaultWidth,
    maxWidth: 1800,
  }), [defaultWidth]);
  // End: width math

  // By default, we sort by the first column that has data (ascending, ignores control/gutter columns)
  const firstDataColumn = React.useMemo(() => columns?.find((col) => !!col.accessor), [columns]);

  const {
    getTableProps,
    getTableBodyProps,
    headerGroups,
    rows,
    prepareRow,
    totalColumnsWidth,
  } = useTable(
    {
      columns,
      data,
      defaultColumn,
      disableSortRemove: true,
      initialState: {
        sortBy: firstDataColumn ? [{ id: firstDataColumn.accessor as string }] : [],
      },
    },
    useBlockLayout, // Note: useFlexLayout would simplify a lot of the sizing code but it's buggy with useResizeColumns.
    useResizeColumns,
    useSortBy,
  );

  const [expanded, setExpanded] = React.useState<string>(null);
  const toggleExpanded = React.useCallback((rowId: string) => {
    if (expanded === rowId) setExpanded(null);
    else setExpanded(rowId);
  }, [expanded]);

  const HeaderRenderer = React.memo<{ column: HeaderGroup }>(
    // eslint-disable-next-line prefer-arrow-callback
    function HeaderRenderer({ column }) {
      return (
        // eslint-disable-next-line react/jsx-key
        <div {...column.getHeaderProps()} className={classes.headerCell}>
          <div
            {...column.getSortByToggleProps()}
            title={String(column.Header) ?? ''}
            className={buildClass(classes.headerCellContents, classes[column.align])}
          >
                  <span className={buildClass(classes.headerLabel, column.align === 'end' && classes.headerLabelRight)}>
                    {column.render('Header')}
                  </span>
            {column.canSort && <ColumnSortButton column={column} />}
          </div>
          {column.canResize && <ColumnResizeHandle column={column} />}
        </div>
      );
    },
    areEqual,
  );

  const CellRenderer = React.memo<{ cell: Cell }>(
    // eslint-disable-next-line prefer-arrow-callback
    function CellRenderer({ cell }) {
      const contents: React.ReactNode = cell.render('Cell');
      const { column: col } = cell;
      // Note: we're not using cell.getCellProps() because it includes CSS that breaks our text-overflow setup.
      // As such, we have to compute the properties we are using ourselves (just width and role really).
      const contClass = buildClass(classes.cellContents, classes[col.align]);
      const cellWidth = Math.max(col.minWidth ?? 0, Math.min(Number(col.width), col.maxWidth ?? Infinity));
      return (
        <div role='cell' className={classes.bodyCell} style={{ width: `${cellWidth}px` }}>
          <div className={contClass}>
            {contents}
          </div>
        </div>
      );
    },
    areEqual,
  );

  const RowRenderer = React.memo<{ index: number, style: React.CSSProperties }>(
    // eslint-disable-next-line prefer-arrow-callback
    function VirtualizedRow({
      index: rowIndex,
      style: vRowStyle,
    }) {
      const row = rows[rowIndex];
      prepareRow(row);
      const className = buildClass(
        classes.bodyRow,
        options.enableRowSelect && classes.bodyRowSelectable,
        options.enableRowSelect && expanded === row.id && classes.bodyRowSelected,
      );
      const onClick = options.enableRowSelect && (() => {
        toggleExpanded(row.id);
        options.onRowSelected?.(expanded === row.id ? null : row.values);
      });
      const rowProps = row.getRowProps({ style: { ...vRowStyle, width: totalColumnsWidth } });
      return (
        // eslint-disable-next-line react/jsx-key
        <div {...rowProps} className={className} onClick={onClick}>
          {row.cells.map((cell) => {
            const key = `r${row.id}-c${cell.column.original?.getColumnName() ?? cell.column.id}`;
            return <CellRenderer key={key} cell={cell} />;
          })}
        </div>
      );
    },
    areEqual,
  );

  const ready = containerWidth > 0 && containerHeight > 0 && defaultWidth > 0;
  if (!ready) return null;

  return (
    <div {...getTableProps()} className={classes.table} style={{ width: containerWidth, height: containerHeight }}>
      <List
        outerRef={scrollContainerRef}
        width={containerWidth}
        height={containerHeight}
        itemCount={rows.length}
        itemSize={ROW_HEIGHT_PX}
        // To make the header sticky (and so it can scroll as part of the same element rather than with JS),
        // we customize the wrapper element that react-window uses for its container to include the header.
        innerElementType={({ children }) => (
          <>
            <Paper
              elevation={options.elevation ?? 0}
              className={classes.tableHead}
              ref={headingsRef}
              style={{ width: totalColumnsWidth }}
            >
              {headerGroups.map((group) => (
                // eslint-disable-next-line react/jsx-key
                <div {...group.getHeaderGroupProps()} className={classes.headerRow}>
                  {group.headers.map((column) => <HeaderRenderer key={column.id} column={column} />)}
                </div>
              ))}
            </Paper>
            <div
              className={classes.tableBody}
              {...getTableBodyProps({
                style: { top: headingsHeight, height: rows.length * ROW_HEIGHT_PX },
              })}
            >
              {children}
            </div>
          </>
        )}
      >
        {RowRenderer}
      </List>
    </div>
  );
};
DataTableImpl.displayName = 'DataTable';

export const DataTable = withAutoSizerContext(DataTableImpl);
