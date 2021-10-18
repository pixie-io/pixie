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
import { VizierTable } from 'app/api';
import { Arguments } from 'app/utils/args-utils';
import { ReactTable, DataTable, DataTableProps } from 'app/components/data-table/data-table';
import { DataType, Relation, SemanticType } from 'app/types/generated/vizierapi_pb';
import { buildClass, CellAlignment } from 'app/components';
import { useTheme, makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { ClusterContext } from 'app/common/cluster-context';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { JSONData } from 'app/containers/format-data/format-data';
import { liveCellRenderer } from 'app/containers/live-data-table/renderers';
import { getSortFunc } from 'app/containers/live-data-table/sort-funcs';
import { useLatestRowCount } from 'app/context/results-context';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';
import { ColumnDisplayInfo, displayInfoFromColumn, titleFromInfo } from './column-display-info';
import ColumnInfo = Relation.ColumnInfo;

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

/** Transforms a table coming from a script into something react-table understands. */
function useConvertedTable(table: VizierTable, propagatedArgs?: Arguments, gutterColumn?: string): ReactTable {
  // Some cell renderers need a bit of extra information that isn't directly related to the table.
  const theme = useTheme();
  const { selectedClusterName: cluster } = React.useContext(ClusterContext);
  const { embedState } = React.useContext(LiveRouteContext);

  // Ensure that useConvertedTable re-renders when the table data is appended to (memoization doesn't see otherwise).
  // Using table.rows.length in memo dependencies below still works, since that value updates on re-render.
  useLatestRowCount(table.name);

  const [displayMap, setDisplayMap] = React.useState<Map<string, ColumnDisplayInfo>>(new Map());

  const convertColumn = React.useCallback((col: ColumnInfo) => {
    const display = displayMap.get(col.getColumnName()) ?? displayInfoFromColumn(col);
    const justify = SemanticAlignmentMap.get(display.semanticType) ?? DataAlignmentMap.get(display.type) ?? 'start';

    const updateDisplay = (newInfo: ColumnDisplayInfo) => {
      displayMap.set(display.columnName, newInfo);
      setDisplayMap(new Map<string, ColumnDisplayInfo>(displayMap));
    };

    const renderer = liveCellRenderer(
      display, updateDisplay, cluster, table, theme, embedState, propagatedArgs);

    const sortFunc = getSortFunc(display);

    const gutterWidth = parseInt(theme.spacing(3), 10); // 24px
    const gutterProps = gutterColumn === display.columnName ? {
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
      Cell({ value }) {
        // TODO(nick,PC-1123): We're not doing width weights yet. Need to. Convert to ratio of default in DataTable?
        // TODO(nick,PC-1102): Head/tail mode (data-table.tsx) for not-the-data-drawer.
        return value != null ? renderer(value) : null;
      },
      original: col,
      align: justify,
      ...gutterProps,
      sortType(a, b) {
        return sortFunc(a.original, b.original);
      },
    };
    // We monitor the length of the data array, not its identity.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [cluster, displayMap, embedState, gutterColumn, propagatedArgs, table.rows.length, theme]);

  const columns = React.useMemo<ReactTable['columns']>(
    () => table.relation
      .getColumnsList()
      .map(convertColumn)
      .sort((a, b) => Number(b.isGutter) - Number(a.isGutter)),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [displayMap]);

  // eslint-disable-next-line react-hooks/exhaustive-deps
  return React.useMemo(() => ({ columns, data: table.rows }), [columns, table.rows.length]);
}

// This one has a sidebar for the currently-selected row, rather than placing it inline like the main data table does.
// It scrolls independently of the table to its left.
const useLiveDataTableStyles = makeStyles((theme: Theme) => createStyles({
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
  details: {
    flex: 1,
    minWidth: '0px',
    minHeight: '0px',
    whiteSpace: 'pre-wrap',
    overflow: 'auto',
    borderLeft: `1px solid ${theme.palette.background.three}`,
    padding: theme.spacing(2),
  },
  detailsHorizontal: {
    borderLeft: 0,
    borderTop: `1px solid ${theme.palette.background.three}`,
  },
}), { name: 'LiveDataTable' });

export const MinimalLiveDataTable = React.memo<{ table: VizierTable }>(function MinimalLiveDataTable({ table }) {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any>|null) => setDetails(row), [setDetails]);

  return (
    <div className={buildClass(classes.root, classes.minimalRoot)}>
      <div className={classes.table}>
        <DataTable table={reactTable} enableRowSelect onRowSelected={onRowSelected} />
      </div>
      {details && (
        <div className={classes.details}>
          <JSONData data={details} multiline />
        </div>
      )}
    </div>
  );
});

export interface LiveDataTableProps extends Pick<DataTableProps, 'onRowsRendered'> {
  table: VizierTable;
  propagatedArgs?: Arguments;
  gutterColumn?: string;
}

const LiveDataTableImpl = React.memo<LiveDataTableProps>(function LiveDataTable({ table, ...options }) {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table, options.propagatedArgs, options.gutterColumn);
  const { width: containerWidth, height: containerHeight } = React.useContext(AutoSizerContext);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any>|null) => setDetails(row), [setDetails]);

  // Determine if we should render row details in a horizontal split or a vertical one based on available space
  const splitMode: 'horizontal'|'vertical' = React.useMemo(
    () => (containerWidth / 4 < 240 ? 'horizontal' : 'vertical'),
    [containerWidth]);

  return (
    <div
      className={buildClass(classes.root, splitMode === 'horizontal' && classes.rootHorizontal)}
      style={{ width: containerWidth, height: containerHeight }}
    >
      <div className={classes.table}>
        <DataTable table={reactTable} enableRowSelect onRowSelected={onRowSelected} enableColumnSelect {...options} />
      </div>
      {details && (
        <div className={buildClass(classes.details, splitMode === 'horizontal' && classes.detailsHorizontal)}>
          <JSONData data={details} multiline />
        </div>
      )}
    </div>
  );
});

export const LiveDataTable = withAutoSizerContext(LiveDataTableImpl);
