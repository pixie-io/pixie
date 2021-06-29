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
import { VariableSizeList, areEqual } from 'react-window';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import { useScrollbarSize } from 'app/utils/use-scrollbar-size';
import { buildClass } from 'app/utils/build-class';

// Augments `@types/react-table` to recognize the plugins we're using
import './react-table-config.d';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';

export interface ReactTable<D extends Record<string, any> = Record<string, any>> {
  columns: Array<Column<D>>;
  data: D[];
}

const useDataTableStyles = makeStyles((theme: Theme) => createStyles({
  // Note: most display and overflow properties here are to align interactions between react-window and react-table.
  table: {
    display: 'block',
    fontSize: theme.typography.pxToRem(15),
    overflow: 'hidden',
  },
  tableHead: {
    position: 'relative', // So it can be pushed left to synchronize with the horizontal content scrolling.
    overflowY: 'auto',
    overflowX: 'hidden',
    minWidth: '100%',
  },
  headerRow: {
    paddingBottom: theme.spacing(1),
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
  tableBody: {},
  bodyRow: {
    '&:not(:last-child)': {
      borderBottom: `1px solid ${theme.palette.background.three}`,
    },
  },
  bodyCell: {
    position: 'relative', // In case anything inside positions absolutely
    display: 'flex',
    alignItems: 'center',
    padding: `0 ${theme.spacing(1)}`,
    height: '100%', // Ensures the border stretches. See cellContents for the rest.
    '&:not(:last-child)': {
      borderRight: `1px solid ${theme.palette.background.three}`,
    },
  },
  cellContents: {
    display: 'inline-block',
    overflow: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    maxHeight: '100%',
    lineHeight: '40px',
  },
  start: { justifyContent: 'flex-start' },
  end: { justifyContent: 'flex-end' },
  center: { justifyContent: 'center' },
  fill: { justifyContent: 'stretch' },
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
  table: ReactTable,
}

const DataTableImpl: React.FC<DataTableProps> = ({ table }) => {
  const classes = useDataTableStyles();

  const { columns, data } = table;

  // Begin: width math
  const virtualScrollRef = React.useRef<VariableSizeList>(null);

  const [scrollbarContainer, setScrollbarContainer] = React.useState<HTMLElement>(null);
  const [scrollLeft, setScrollLeft] = React.useState(0);
  const scrollContainerRef = React.useCallback((el) => setScrollbarContainer(el), []);
  const { width: scrollbarWidth } = useScrollbarSize(scrollbarContainer);

  React.useEffect(() => {
    const el = scrollbarContainer;
    if (!el) return () => {};

    const listener = () => setScrollLeft(el.scrollLeft);
    el.addEventListener('scroll', listener);
    return () => el?.removeEventListener('scroll', listener);
  }, [scrollbarContainer]);

  const { width: containerWidth, height: containerHeight } = React.useContext(AutoSizerContext);

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
    state: reactTableState,
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

  const CellRenderer = React.memo<{ cell: Cell }>(
    // eslint-disable-next-line prefer-arrow-callback
    function CellRenderer({ cell }) {
      const contents: React.ReactNode = cell.render('Cell');
      const { column: col } = cell;
      // Note: we're not using cell.getCellProps() because it includes CSS that breaks our text-overflow setup.
      // As such, we have to compute the properties we are using ourselves (just width and role really).
      const cellClass = buildClass(classes.bodyCell, classes[col.align]);
      const cellWidth = Math.max(col.minWidth ?? 0, Math.min(Number(col.width), col.maxWidth ?? Infinity));
      return (
        <div role='cell' className={cellClass} style={{ width: `${cellWidth}px` }}>
          <div className={classes.cellContents}>
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
      return (
        // eslint-disable-next-line react/jsx-key
        <div {...row.getRowProps({ style: { ...vRowStyle, width: totalColumnsWidth } })} className={classes.bodyRow}>
          {row.cells.map((cell) => (
            <CellRenderer key={cell.column.original?.getColumnName() ?? cell.column.id} cell={cell} />
          ))}
        </div>
      );
    },
    areEqual,
  );

  // VariableSizeList needs to be informed when columns resize, otherwise it doesn't update until the next scroll event.
  React.useEffect(() => {
    virtualScrollRef.current?.resetAfterIndex(0);
  }, [reactTableState.columnResizing]);

  /*
   * A few notes on semantic HTML and WAI-ARIA with this table:
   * - getTableProps (+friends) set `role` so screen readers still see tables; also sets the `key` prop for React.
   * - <VariableSizeList>, even with innerElementType/outerElementType, creates more nesting levels than tables allow.
   * - Doing this rather than <table> lets the table's header stay visible while its body scrolls.
   */
  const ready = containerWidth > 0 && containerHeight > 0 && defaultWidth > 0;
  if (!ready) return null;
  return (
    <div {...getTableProps()} className={classes.table} style={{ width: containerWidth, height: containerHeight }}>
      <div
        className={classes.tableHead}
        ref={headingsRef}
        style={{ width: Math.max(totalColumnsWidth, containerWidth - scrollbarWidth), left: -scrollLeft }}
      >
        {headerGroups.map((group) => (
          // eslint-disable-next-line react/jsx-key
          <div {...group.getHeaderGroupProps()} className={classes.headerRow}>
            {group.headers.map((column) => (
              // eslint-disable-next-line react/jsx-key
              <div {...column.getHeaderProps()} className={classes.headerCell}>
                <div
                  {...column.getSortByToggleProps()}
                  title={column.Header as string}
                  className={buildClass(classes.headerCellContents, classes[column.align])}
                >
                  <span className={buildClass(classes.headerLabel, column.align === 'end' && classes.headerLabelRight)}>
                    {column.render('Header')}
                  </span>
                  {column.canSort && <ColumnSortButton column={column} />}
                </div>
                {column.canResize && <ColumnResizeHandle column={column} />}
              </div>
            ))}
          </div>
        ))}
      </div>
      <div {...getTableBodyProps()} className={classes.tableBody}>
        {/* Need to wait for the headings to load in so we can size the scroller correctly */}
        {headingsHeight > 0 && (
          <VariableSizeList
            ref={virtualScrollRef}
            outerRef={scrollContainerRef}
            width={containerWidth}
            height={containerHeight - headingsHeight}
            itemCount={rows.length}
            estimatedItemSize={40}
            // TODO(nick,PC-1050): This needs to be based on expansion state. Gets one param: the row index.
            itemSize={() => 40}
            overscanCount={3} // Limits flashing when scrolling quickly
          >
            {RowRenderer}
          </VariableSizeList>
        )}
      </div>
    </div>
  );
};
DataTableImpl.displayName = 'DataTable';

export const DataTable = withAutoSizerContext(DataTableImpl);
