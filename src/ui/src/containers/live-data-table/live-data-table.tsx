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

import { useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Column as ReactTableColumn } from 'react-table';

import { VizierTable } from 'app/api';
import { ClusterContext } from 'app/common/cluster-context';
import { buildClass, CellAlignment } from 'app/components';
import { ReactTable, DataTable, DataTableProps } from 'app/components/data-table/data-table';
import { getLiveCellRenderer } from 'app/containers/live-data-table/renderers';
import { getSortFunc } from 'app/containers/live-data-table/sort-funcs';
import { useLatestRowCount } from 'app/context/results-context';
import { DataType, Relation, SemanticType } from 'app/types/generated/vizierapi_pb';
import { Arguments } from 'app/utils/args-utils';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';

import { ColumnDisplayInfo, displayInfoFromColumn, titleFromInfo } from './column-display-info';
import { DetailPane } from './data-table-details';

// Note: if an alignment exists for both a column's semantic type and its data type, the semantic type takes precedence.
const SemanticAlignmentMap = new Map<SemanticType, CellAlignment>(
  [
    [SemanticType.ST_QUANTILES, 'fill'],
    [SemanticType.ST_DURATION_NS_QUANTILES, 'fill'],
  ],
);

const DataAlignmentMap = new Map<DataType, CellAlignment>(
  [
    [DataType.BOOLEAN, 'center'],
    [DataType.INT64, 'end'],
    [DataType.UINT128, 'start'],
    [DataType.FLOAT64, 'end'],
    [DataType.STRING, 'start'],
    [DataType.TIME64NS, 'end'],
  ],
);

export type CompleteColumnDef =
  ReactTableColumn<Record<string, any>>
  & { Cell: ({ value }: { value: any }) => (JSX.Element | null) };

/** Transforms a table coming from a script into something react-table understands. */
function useConvertedTable(
  table: VizierTable,
  propagatedArgs?: Arguments,
  gutterColumns: Array<string | CompleteColumnDef> = [],
): ReactTable {
  // Some cell renderers need a bit of extra information that isn't directly related to the table.
  const theme = useTheme();
  const { selectedClusterName: cluster } = React.useContext(ClusterContext);

  // Ensure that useConvertedTable re-renders when the table data is appended to (memoization doesn't see otherwise).
  // Using table.rows.length in memo dependencies below still works, since that value updates on re-render.
  useLatestRowCount(table.name);

  const [displayMap, setDisplayMap] = React.useState<Map<string, ColumnDisplayInfo>>(new Map());

  const gutterWidth = parseInt(theme.spacing(4), 10); // 32px

  const convertColumn = React.useCallback<(col: Relation.ColumnInfo) => CompleteColumnDef>((col) => {
    const display = displayMap.get(col.getColumnName()) ?? displayInfoFromColumn(col);
    const justify = SemanticAlignmentMap.get(display.semanticType) ?? DataAlignmentMap.get(display.type) ?? 'start';

    const updateDisplay = (newInfo: ColumnDisplayInfo) => {
      displayMap.set(display.columnName, newInfo);
      setDisplayMap(new Map<string, ColumnDisplayInfo>(displayMap));
    };

    const Renderer = getLiveCellRenderer(table, display, updateDisplay, cluster, propagatedArgs);

    const sortFunc = getSortFunc(display);

    const gutterProps = gutterColumns.includes(display.columnName) ? {
      isGutter: true,
      minWidth: gutterWidth,
      width: gutterWidth,
      maxWidth: gutterWidth,
      Header: '\xa0', // nbsp
      disableSortBy: true,
      disableFilters: true,
      disableResizing: true,
    } : { isGutter: false };

    return {
      Header: titleFromInfo(display),
      accessor: col.getColumnName(),
      // TODO(nick,PC-1123): We're not doing width weights yet. Need to. Convert to ratio of default in DataTable?
      // TODO(nick,PC-1102): Head/tail mode (data-table.tsx) for not-the-data-drawer.
      // eslint-disable-next-line react/display-name
      Cell({ value }) {
        return value != null ? <Renderer data={value} /> : null;
      },
      original: col,
      align: justify,
      ...gutterProps,
      sortType(a, b) {
        return sortFunc(a.original, b.original);
      },
    };
  }, [table, cluster, displayMap, gutterColumns, propagatedArgs, gutterWidth]);

  const customGutters: CompleteColumnDef[] = React.useMemo(
    () => gutterColumns
      .filter(c => c && typeof c === 'object')
      .map((c: CompleteColumnDef) => ({
        ...c,
        isGutter: true,
        minWidth: gutterWidth,
        width: gutterWidth,
        maxWidth: gutterWidth,
        Header: '\xa0', // nbsp
        disableSortBy: true,
        disableFilters: true,
        disableResizing: true,
      })),
    [gutterColumns, gutterWidth]);

  const columns = React.useMemo<ReactTable['columns']>(
    () => (
      [
        table.relation.getColumnsList().map(convertColumn),
        customGutters,
      ].flat().sort((a, b) => Number(b.isGutter) - Number(a.isGutter))
    ),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [displayMap, customGutters]);

  const oldestRow = table.rows[0]; // Used to force updates when row limit is reached
  return React.useMemo(
    () => ({ columns, data: table.rows }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [columns, table.rows.length, oldestRow],
  );
}

// This one has a sidebar for the currently-selected row, rather than placing it inline like the main data table does.
// It scrolls independently of the table to its left.
const useLiveDataTableStyles = makeStyles(createStyles({
  root: {
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'stretch',
    alignItems: 'stretch',
    overflow: 'hidden',
    position: 'relative',
  },
  minimalRoot: {
    height: '100%',
    width: '100%',
  },
  rootHorizontal: {
    flexFlow: 'column nowrap',
  },
  table: {
    flex: 3,
    overflow: 'hidden',
    width: '100%', // It's using an AutoSizer that has a natural width/height of 0. Need to force it to use space.
  },
}), { name: 'LiveDataTable' });

export const MinimalLiveDataTable = React.memo<{ table: VizierTable }>(({ table }) => {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any> | null) => setDetails(row), [setDetails]);
  const updateSelection = React.useRef<(id: string | null) => void>();
  const closeDetails = React.useCallback(() => {
    if (updateSelection.current) {
      updateSelection.current(null);
      setDetails(null);
    }
  }, [updateSelection]);

  return (
    <div className={buildClass(classes.root, classes.minimalRoot)}>
      <div className={classes.table}>
        <DataTable
          table={reactTable}
          enableRowSelect
          onRowSelected={onRowSelected}
          updateSelection={updateSelection}
        />
      </div>
      <DetailPane details={details} closeDetails={closeDetails} />
    </div>
  );
});
MinimalLiveDataTable.displayName = 'MinimalLiveDataTable';

export interface LiveDataTableProps extends Pick<DataTableProps, 'onRowsRendered'> {
  table: VizierTable;
  propagatedArgs?: Arguments;
  gutterColumns?: Array<string | CompleteColumnDef>;
  setExternalControls?: React.RefCallback<React.ReactNode>;
}

const LiveDataTableImpl = React.memo<LiveDataTableProps>(({ table, ...options }) => {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table, options.propagatedArgs, options.gutterColumns);
  const { width: containerWidth, height: containerHeight } = React.useContext(AutoSizerContext);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any> | null) => setDetails(row), [setDetails]);
  const updateSelection = React.useRef<(id: string | null) => void>();
  const closeDetails = React.useCallback(() => {
    if (updateSelection.current) {
      updateSelection.current(null);
      setDetails(null);
    }
  }, [updateSelection]);

  // Determine if we should render row details in a horizontal split or a vertical one based on available space
  const splitMode: 'horizontal' | 'vertical' = React.useMemo(
    () => (containerWidth / 4 < 240 ? 'horizontal' : 'vertical'),
    [containerWidth]);

  return (
    <div
      className={buildClass(classes.root, splitMode === 'horizontal' && classes.rootHorizontal)}
      style={{ width: containerWidth, height: containerHeight }}
    >
      <div className={classes.table}>
        <DataTable
          table={reactTable}
          enableRowSelect
          onRowSelected={onRowSelected}
          updateSelection={updateSelection}
          enableColumnSelect
          {...options}
        />
      </div>
      <DetailPane details={details} closeDetails={closeDetails} splitMode={splitMode} />
    </div>
  );
});
LiveDataTableImpl.displayName = 'LiveDataTable';

export const LiveDataTable = withAutoSizerContext(LiveDataTableImpl);
