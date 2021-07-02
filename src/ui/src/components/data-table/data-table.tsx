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

import { buildClass } from 'app/utils/build-class';
import * as React from 'react';
import { DraggableCore } from 'react-draggable';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';

import {
  Column,
  defaultTableRowRenderer,
  IndexRange,
  SortDirection,
  SortDirectionType,
  Table,
  TableCellProps,
  TableCellRenderer,
  TableHeaderProps,
  TableHeaderRenderer,
  TableRowProps,
  TableRowRenderer,
} from 'react-virtualized';

import {
  alpha,
  makeStyles,
  Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Tooltip from '@material-ui/core/Tooltip';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import MenuIcon from '@material-ui/icons/Menu';
import { CSSProperties, MutableRefObject } from 'react';
import { clamp } from 'app/utils/math';
import { withAutoSizerContext, AutoSizerContext } from 'app/utils/autosizer';
import { ExpandedIcon } from 'app/components/icons/expanded';
import { UnexpandedIcon } from 'app/components/icons/unexpanded';
import { Button, Checkbox, FormControlLabel } from '@material-ui/core';
import { useScrollPosition } from 'app/utils/use-scroll-position';
import {
  MAX_COL_PX_WIDTH,
  MIN_COL_PX_WIDTH,
  userResizeColumn,
  StartingRatios,
  ColWidthOverrides,
  tableWidthLimits,
} from './table-resizer';

const EXPANDED_ROW_HEIGHT = 300;

const useStyles = makeStyles((theme: Theme) => createStyles({
  table: {
    color: theme.palette.text.primary,
    '& > .ReactVirtualized__Table__headerRow': {
      ...theme.typography.caption,
      display: 'flex',
      width: 'var(--header-width) !important',
      position: 'relative',
    },
    '& > .ReactVirtualized__Table__Grid': {
      overflowX: 'auto !important',
      '& > .ReactVirtualized__Grid__innerScrollContainer': {
        maxWidth: 'var(--table-width) !important',
        width: 'var(--table-width) !important',
      },
    },
  },
  row: {
    borderBottom: `solid 2px ${theme.palette.background.three}`,
    '& > .ReactVirtualized__Table__rowColumn:first-of-type': {
      marginLeft: 0,
      marginRight: 0,
    },
    '&:hover $hidden': {
      display: 'flex',
    },
    '& > .ReactVirtualized__Table__rowColumn': {
      borderRight: `solid 1px ${theme.palette.background.three}`,
    },
    '& > .ReactVirtualized__Table__rowColumn:last-child': {
      borderRight: 'none',
    },
    display: 'flex',
    fontSize: '15px',
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
    alignItems: 'baseline',
    height: theme.spacing(6),
    lineHeight: theme.spacing(6),
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
    height: theme.spacing(5),
    lineHeight: theme.spacing(5),
  },
  clickable: {
    cursor: 'pointer',
  },
  highlighted: {
    backgroundColor: theme.palette.foreground.grey3,
  },
  highlightable: {
    '&:hover': {
      backgroundColor: `${alpha(theme.palette.foreground.grey2, 0.42)}`,
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
  fill: {
    '& > *': {
      width: '100%',
    },
  },
  sortIcon: {
    width: theme.spacing(3),
    paddingLeft: theme.spacing(1),
  },
  sortIconHidden: {
    width: theme.spacing(3),
    opacity: '0.2',
    paddingLeft: theme.spacing(1),
  },
  headerTitle: {
    ...theme.typography.h4,
    height: '100%',
    display: 'flex',
    alignItems: 'center',
    flex: 'auto',
    overflow: 'hidden',
    background: 'transparent',
    border: 'none',
    color: 'inherit',
    cursor: 'pointer',
    textTransform: 'uppercase',
  },
  gutterHeader: {
    display: 'flex',
    width: '100%',
    overflow: 'hidden',
  },
  gutterCell: {
    paddingLeft: '2px',
    flex: 'auto',
    alignItems: 'center',
    minWidth: theme.spacing(2.5),
    display: 'flex',
    height: '100%',
    borderRight: 'none !important',
  },
  expanderHeader: {
    display: 'flex',
    justifyContent: 'center',
    position: 'relative',
    width: '100%',
    overflow: 'hidden',

    '& svg': {
      fontSize: theme.typography.h3.fontSize,
    },
  },
  noPointerEvents: {
    pointerEvents: 'none',
  },
  expanderCell: {
    paddingLeft: '0px',
    flex: 'auto',
    alignItems: 'center',
    // TODO(michelle/zasgar): Fix this.
    overflow: 'visible',
    minWidth: theme.spacing(2.5),
    display: 'flex',
    height: '100%',
    borderRight: 'none !important',
  },
  dragHandle: {
    flex: '0 0 12px',
    display: 'flex',
    flexDirection: 'column',
    justifyContent: 'center',
    alignItems: 'center',
    cursor: 'col-resize',
    // Move the handle's center, rather than its right edge, to line up with the column's right border
    transform: 'translate(50%, -1px)',
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

export type ExpandedRows = Record<number, boolean>;

export type CellAlignment = 'center' | 'start' | 'end' | 'fill';

export interface ColumnProps {
  dataKey: string;
  label: string;
  width?: number;
  align?: CellAlignment;
  cellRenderer?: (data: any) => React.ReactNode;
}

interface DataTableProps {
  columns: ColumnProps[];
  gutterColumn?: ColumnProps;
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

interface ColumnDisplaySelectorProps {
  options: string[];
  selected: string[];
  onChange: React.Dispatch<React.SetStateAction<string[]>>;
}

const ColumnDisplaySelector: React.FC<ColumnDisplaySelectorProps> = ({ options, selected, onChange }) => {
  const classes = useStyles();
  const [open, setOpen] = React.useState<boolean>(false);
  const anchorEl = React.useRef<HTMLButtonElement>(null);
  const toggleOption = (dataKey: string) => {
    if (selected.includes(dataKey)) {
      // Don't allow deselecting every column, at least one must always be available or the table breaks.
      if (selected.length > 1) {
        onChange(selected.filter((k) => k !== dataKey));
      }
    } else {
      onChange([...selected, dataKey]);
    }
  };
  return (
    <>
      <Menu open={open} anchorEl={anchorEl.current} onBackdropClick={() => setOpen(false)}>
        {options.map((dataKey) => (
          <MenuItem key={dataKey} button onClick={() => toggleOption(dataKey)}>
            <FormControlLabel
              className={classes.noPointerEvents}
              control={<Checkbox color='secondary' disableRipple checked={selected.includes(dataKey)} />}
              label={dataKey}
            />
          </MenuItem>
        ))}
      </Menu>
      <Button onClick={() => setOpen(!open)} ref={anchorEl}>
        <MenuIcon />
      </Button>
    </>
  );
};

const InternalDataTable: React.FC<DataTableProps> = ({
  columns,
  gutterColumn,
  onRowClick = () => {},
  rowCount,
  rowGetter,
  compact = false,
  resizableColumns = false,
  expandable = false,
  expandedRenderer = () => <></>,
  onSort = () => {},
  highlightedRow = -1,
  onRowsRendered = () => {},
}) => {
  const classes = useStyles();

  const { width, height } = React.useContext(AutoSizerContext);
  const [widthOverrides, setColumnWidthOverride] = React.useState<
  ColWidthOverrides
  >({});
  const tableRef: MutableRefObject<Table> = React.useRef(null);

  const [sortState, setSortState] = React.useState<SortState>({
    dataKey: '',
    direction: SortDirection.DESC,
  });
  const [expandedRowState, setExpandedRowState] = React.useState<ExpandedRows>(
    {},
  );

  const [shownColumns, setShownColumns] = React.useState<string[]>([]);
  React.useEffect(() => {
    setShownColumns(columns.map((c) => c.dataKey));
  }, [columns]);

  const totalWidth = React.useRef<number>(0);
  const colTextWidthRatio = React.useMemo<{ [dataKey: string]: number }>(() => {
    totalWidth.current = 0;
    const colsWidth: { [dataKey: string]: number } = {};
    const measuringContext = document.createElement('canvas').getContext('2d');
    columns.filter((c) => shownColumns.includes(c.dataKey)).forEach((col) => {
      let w = col.width || null;
      if (!w) {
        const row = rowGetter(0);
        const text = String(row[col.dataKey]);
        const textWidth = measuringContext.measureText(text).width;
        w = clamp(textWidth, MIN_COL_PX_WIDTH, MAX_COL_PX_WIDTH);
      }

      // We add 2 to the header width to accommodate type/sort icons.
      const headerWidth = col.label.length + 2;
      colsWidth[col.dataKey] = clamp(
        Math.max(headerWidth, w),
        MIN_COL_PX_WIDTH,
        MAX_COL_PX_WIDTH,
      );
      totalWidth.current += colsWidth[col.dataKey];
    });
    // Ensure the total is at least as wide as the available space, and not so wide as to violate column max widths.
    const [minTotal, maxTotal] = tableWidthLimits(shownColumns.length, width);
    totalWidth.current = clamp(totalWidth.current, minTotal, maxTotal);

    const ratio: { [dataKey: string]: number } = {};
    const prevSum = Object.values(colsWidth).reduce((a, c) => a + c, 0);
    Object.keys(colsWidth).forEach((colsWidthKey) => {
      ratio[colsWidthKey] = colsWidth[colsWidthKey] / (prevSum || 1);
    });
    return ratio;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [columns, shownColumns, rowGetter, rowCount, width]);

  const rowGetterWrapper = React.useCallback(({ index }) => rowGetter(index), [
    rowGetter,
  ]);

  const cellRenderer: TableCellRenderer = React.useCallback(
    (props: TableCellProps) => (
      <div
        className={buildClass(classes.cellWrapper, classes[props.columnData.align])}
      >
        <div className={classes.innerCell}>
          {props.columnData.cellRenderer
            && props.columnData.cellRenderer(props.cellData)}
          {!props.columnData.cellRenderer && (
            <span className={classes.cellText}>{String(props.cellData)}</span>
          )}
        </div>
      </div>
    ),
    [classes],
  );

  const defaultCellHeight = compact ? 40 : 48;
  const computeRowHeight = React.useCallback(
    ({ index }) => (expandedRowState[index] ? EXPANDED_ROW_HEIGHT : defaultCellHeight),
    [defaultCellHeight, expandedRowState],
  );

  const onSortWrapper = React.useCallback(
    ({ sortBy, sortDirection }) => {
      if (sortBy) {
        const nextSortState = { dataKey: sortBy, direction: sortDirection };
        if (
          sortState.dataKey !== sortBy
          || sortState.direction !== sortDirection
        ) {
          setSortState(nextSortState);
        }
        onSort(nextSortState);
        tableRef.current.forceUpdateGrid();
      }
    },
    [onSort, sortState],
  );

  React.useEffect(() => {
    let sortKey;
    for (let i = 0; i < columns.length; ++i) {
      // Don't use an empty label which may be a expander column.
      if (columns[i].label) {
        sortKey = columns[i].dataKey;
        break;
      }
    }
    if (sortKey && !sortState.dataKey) {
      onSortWrapper({ sortBy: sortKey, sortDirection: SortDirection.ASC });
    } else {
      onSortWrapper({
        sortBy: sortState.dataKey,
        sortDirection: sortState.direction,
      });
    }
  }, [columns, shownColumns, onSortWrapper, sortState]);

  const onRowClickWrapper = React.useCallback(
    ({ index }) => {
      if (expandable) {
        setExpandedRowState((state) => {
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
    },
    [onRowClick, expandable],
  );

  const getRowClass = React.useCallback(
    ({ index }) => {
      if (index === -1) {
        return null;
      }
      return buildClass(
        classes.row,
        onRowClick && classes.clickable,
        onRowClick && classes.highlightable,
        index === highlightedRow && classes.highlighted,
      );
    },
    [highlightedRow, onRowClick, classes],
  );

  const resizeColumn = React.useCallback(
    ({ dataKey, deltaX }) => {
      setColumnWidthOverride((state) => {
        const startingRatios: StartingRatios = columns
          .filter((c) => shownColumns.includes(c.dataKey))
          .map((col) => {
            const key = col.dataKey;
            const isDefault = state[key] == null;
            return {
              key,
              isDefault,
              ratio: state[key] || colTextWidthRatio[key],
            };
          });
        const { newTotal, sizes } = userResizeColumn(
          dataKey,
          deltaX,
          startingRatios,
          totalWidth.current,
          width,
        );
        totalWidth.current = newTotal;
        return sizes;
      });
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [width, colTextWidthRatio, columns, shownColumns],
  );

  const colIsResizable = (idx: number): boolean => (resizableColumns || true) && idx !== shownColumns.length - 1;

  const resetColumn = (dataKey: string) => {
    setColumnWidthOverride((overrides) => {
      const colIdx = columns.findIndex((col) => col.dataKey === dataKey);
      if (colIdx === -1) {
        return overrides;
      }

      const newOverrides = { ...overrides };
      delete newOverrides[dataKey];

      const nextKey = columns[colIdx + 1]?.dataKey;
      if (nextKey) {
        delete newOverrides[nextKey];
      }

      return newOverrides;
    });
    // The reset above can cause the total width not to add up anymore, so force a rescale of the sum.
    // This results in only the columns on either side of the drag handle resetting, with the rest not moving.
    resizeColumn({ dataKey, deltaX: 0 });
  };

  const tableWrapper = React.useRef<HTMLDivElement>(null);

  // This detects both the presence and width of the vertical scrollbar, to avoid miscalculating header dimensions.
  // Note that on mobile devices, Safari, and Chrome (by default but configurable), the scrollbar is an overlay that
  // doesn't affect the geometry of the element it scrolls. In that case, the scrollbar has a width of 0.
  const scrollbarWidth = React.useMemo<number>(() => {
    const scroller: HTMLElement = tableWrapper.current?.querySelector(
      '.ReactVirtualized__Table__Grid',
    );
    if (!scroller) return 0;
    return scroller.offsetWidth - scroller.clientWidth;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [
    tableWrapper.current,
    width,
    totalWidth.current,
    colTextWidthRatio,
    widthOverrides,
    columns,
    shownColumns,
  ]);

  const [wasAtTop, setWasAtTop] = React.useState<boolean>(false);
  const [wasAtBottom, setWasAtBottom] = React.useState<boolean>(false);

  const scroller = React.useMemo(
    () => tableWrapper.current?.querySelector('.ReactVirtualized__Table__Grid'),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [tableWrapper.current],
  );

  React.useEffect(() => {
    // Using direct DOM manipulation as we're messing with internals of a third-party component that doesn't expose refs
    const header: HTMLDivElement = tableWrapper.current?.querySelector(
      '.ReactVirtualized__Table__headerRow',
    );
    if (!header || !scroller) return () => {};

    const listener = () => {
      if (header && scroller) {
        header.style.left = `${-1 * (scroller.scrollLeft ?? 0)}px`;
      }
      if (scroller) {
        // For convenience, snap into head/tail mode when the user scrolls close enough to the edge.
        // This also works around the react-virtualized tendency to flicker the scroll position right after changing it.
        setWasAtTop(scroller.scrollTop < defaultCellHeight / 2);
        const scrollBottomLimit = scroller.scrollHeight - scroller.clientHeight - defaultCellHeight / 2;
        setWasAtBottom(scroller.scrollTop >= scrollBottomLimit);
      }
    };

    scroller.addEventListener('scroll', listener);
    return () => {
      if (scroller) scroller.removeEventListener('scroll', listener);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tableWrapper.current, computeRowHeight, rowCount, scroller]);

  React.useEffect(() => {
    // If the table is already scrolled to the top or the bottom, keep it that way when the data changes
    if (!scroller) return;
    /* TODO(nick): On long renders (like adding a thousand results at once), the scrollHeight seen here lags a bit
     *  behind the actual value painted on screen briefly. If that happens, we incorrectly scroll out of head/tail mode.
     *  When implementing streaming window rendering, check if the performance improvement there fixes this.
     *  If it doesn't, the first suspect is the windowing logic in <Table> - it might have a race condition.
     */
    if (wasAtTop) {
      scroller.scrollTo(scroller.scrollLeft, 0);
    } else if (wasAtBottom) {
      scroller.scrollTo({
        left: scroller.scrollLeft,
        top: scroller.scrollHeight - scroller.clientHeight,
        behavior: 'auto',
      });
    }
  }, [
    scroller?.clientHeight,
    scroller?.scrollHeight,
    scroller,
    wasAtBottom,
    wasAtTop,
  ]);

  const headerRendererCommon: TableHeaderRenderer = React.useCallback(
    (props) => {
      const sort = () => onSortWrapper({
        sortBy: props.dataKey,
        sortDirection:
            props.sortDirection === SortDirection.ASC
              ? SortDirection.DESC
              : SortDirection.ASC,
      });

      let sortIcon = <UpIcon className={classes.sortIconHidden} />;
      if (
        props.sortBy === props.dataKey
        && props.sortDirection === SortDirection.ASC
      ) {
        sortIcon = <UpIcon className={classes.sortIcon} />;
      } else if (
        props.sortBy === props.dataKey
        && props.sortDirection === SortDirection.DESC
      ) {
        sortIcon = <DownIcon className={classes.sortIcon} />;
      }

      const headerStyle = buildClass(
        [classes.headerTitle],
        [classes[props.columnData.align]],
      );

      return (
        <button type='button' className={headerStyle} onClick={sort}>
          <Tooltip title={props.label}>
            <span className={classes.cellText}>{props.label}</span>
          </Tooltip>
          {sortIcon}
        </button>
      );
    },
    [classes, onSortWrapper],
  );

  const headerRenderer: TableHeaderRenderer = React.useCallback(
    (props: TableHeaderProps) => (
      <React.Fragment key={props.dataKey}>
        {headerRendererCommon(props)}
      </React.Fragment>
    ),
    [headerRendererCommon],
  );

  const gutterHeaderRenderer: TableHeaderRenderer = React.useCallback(
    (props: TableHeaderProps) => (
      props.columnData ? <div key={props.dataKey} className={classes.gutterHeader} /> : <></>
    ),
    [classes.gutterHeader],
  );

  const gutterCellRenderer: TableCellRenderer = React.useCallback(
    (props: TableCellProps) => (
      <>
        <div className={classes.gutterCell}>
          {props.columnData?.cellRenderer
              && props.columnData.cellRenderer(props.cellData)}
        </div>
      </>
    ),
    [classes.gutterCell],
  );

  const expanderHeaderRenderer: TableHeaderRenderer = React.useCallback(
    (props: TableHeaderProps) => (
      <div key={props.dataKey} className={classes.expanderHeader}>
        <ColumnDisplaySelector
          options={columns.map((c) => c.dataKey)}
          selected={shownColumns}
          onChange={setShownColumns}
        />
      </div>
    ),
    [columns, shownColumns, classes.expanderHeader],
  );

  const expanderCellRenderer: TableCellRenderer = React.useCallback(
    (props: TableCellProps) => {
      // Hide the icon by default unless:
      //  1. It's been expanded.
      //  2. The row has been highlighted.
      const cls = buildClass(
        classes.expanderCell,
        !(
          highlightedRow === props.rowIndex || expandedRowState[props.rowIndex]
        ) && classes.hidden,
      );
      const icon = expandedRowState[props.rowIndex] ? (
        <ExpandedIcon />
      ) : (
        <UnexpandedIcon />
      );
      return (
        <>
          <div className={cls}>{icon}</div>
        </>
      );
    },
    [highlightedRow, expandedRowState, classes],
  );

  const { scrollLeft: throttledScrollLeft } = useScrollPosition(scroller);

  const rowRenderer: TableRowRenderer = React.useCallback(
    (props: TableRowProps) => {
      const { style: rowStyle } = props;
      rowStyle.width = '100%';
      const expansionStyle: React.CSSProperties = {
        width: scroller?.clientWidth ?? '100%',
        position: 'relative',
        left: throttledScrollLeft,
      };
      return (
        <div className={classes.rowContainer} key={props.key} style={rowStyle}>
          {defaultTableRowRenderer({
            ...props,
            key: '',
            style: { height: defaultCellHeight },
          })}

          {expandable && expandedRowState[props.index] && (
            <div className={classes.expandedCell} style={expansionStyle}>
              {expandedRenderer(rowGetter(props.index))}
            </div>
          )}
        </div>
      );
    },
    [
      classes,
      defaultCellHeight,
      expandedRowState,
      expandedRenderer,
      expandable,
      rowGetter,
      scroller?.clientWidth,
      throttledScrollLeft,
    ],
  );

  const headerRendererWithDrag: TableHeaderRenderer = React.useCallback(
    (props: TableHeaderProps) => {
      const { dataKey } = props;
      return (
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
            <span
              className={classes.dragHandle}
              onDoubleClick={() => {
                resetColumn(dataKey);
              }}
            >
              &#8942;
            </span>
          </DraggableCore>
        </React.Fragment>
      );
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [classes, headerRendererCommon, resizeColumn],
  );
  const gutterClass = buildClass(compact && classes.compact, classes.gutterCell);
  const expanderClass = buildClass(compact && classes.compact, classes.expanderCell);
  return (
    <div ref={tableWrapper}>
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
        style={
          {
            '--table-width': `${totalWidth.current - scrollbarWidth}px`,
            '--header-width': `${totalWidth.current}px`,
          } as CSSProperties & Record<string, string>
        }
        sortDirection={sortState.direction}
        sortBy={sortState.dataKey}
        onRowsRendered={onRowsRendered}
      >
        {expandable && (
          <Column
            key='expander'
            dataKey='expander'
            label=''
            headerClassName={expanderClass}
            className={expanderClass}
            headerRenderer={expanderHeaderRenderer}
            cellRenderer={expanderCellRenderer}
            width={4 /* width for chevron */}
            columnData={null}
          />
        )}
        {gutterColumn && (
          <Column
            key={gutterColumn.dataKey}
            dataKey={gutterColumn.dataKey}
            label=''
            headerClassName={gutterClass}
            className={gutterClass}
            headerRenderer={gutterHeaderRenderer}
            cellRenderer={gutterCellRenderer}
            width={26 /* width for gutter */}
            columnData={gutterColumn}
          />
        )}
        {columns.filter((col) => shownColumns.includes(col.dataKey)).map((col, i) => {
          const className = buildClass(
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
              headerRenderer={
                colIsResizable(i) ? headerRendererWithDrag : headerRenderer
              }
              cellRenderer={cellRenderer}
              width={
                (widthOverrides[col.dataKey]
                  || colTextWidthRatio[col.dataKey]) * totalWidth.current
              }
              columnData={col}
            />
          );
        })}
      </Table>
    </div>
  );
};
InternalDataTable.displayName = 'DataTable';

export const DataTable = withAutoSizerContext(InternalDataTable);
