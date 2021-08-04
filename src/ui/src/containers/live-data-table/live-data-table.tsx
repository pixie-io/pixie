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
import { Table as VizierTable } from 'app/api';
import { Arguments } from 'app/utils/args-utils';
import { dataFromProto } from 'app/utils/result-data-utils';
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
import { ColumnDisplayInfo, displayInfoFromColumn, titleFromInfo } from './column-display-info';
import { parseRows } from './parsers';
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

function rowsFromVizierTable(table: VizierTable): Array<Record<string, any>> {
  const semanticTypeMap = table.relation.getColumnsList().reduce((acc, col) => {
    acc.set(col.getColumnName(), col.getColumnSemanticType());
    return acc;
  }, new Map<string, SemanticType>());

  return parseRows(semanticTypeMap, dataFromProto(table.relation, table.data));
}

/** Transforms a table coming from a script into something react-table understands. */
function useConvertedTable(table: VizierTable, propagatedArgs?: Arguments, gutterColumn?: string): ReactTable {
  // Some cell renderers need a bit of extra information that isn't directly related to the table.
  const theme = useTheme();
  const { selectedClusterName: cluster } = React.useContext(ClusterContext);
  const { embedState } = React.useContext(LiveRouteContext);

  // eslint-disable-next-line react-hooks/exhaustive-deps
  const rows = React.useMemo(() => rowsFromVizierTable(table), [table.data]);

  const [displayMap, setDisplayMap] = React.useState<Map<string, ColumnDisplayInfo>>(new Map());

  const convertColumn = (col: ColumnInfo) => {
    const display = displayMap.get(col.getColumnName()) ?? displayInfoFromColumn(col);
    const justify = SemanticAlignmentMap.get(display.semanticType) ?? DataAlignmentMap.get(display.type) ?? 'start';

    const updateDisplay = (newInfo: ColumnDisplayInfo) => {
      displayMap.set(display.columnName, newInfo);
      setDisplayMap(new Map<string, ColumnDisplayInfo>(displayMap));
    };

    const renderer = liveCellRenderer(display, updateDisplay, true, theme, cluster, rows, embedState, propagatedArgs);

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
        // TODO(nick,PC-1050): We're not doing width weights yet. Need to. Convert to ratio of default in DataTable?
        // TODO(nick,PC-1050): Head/tail mode (data-table.tsx) for not-the-data-drawer.
        // TODO(nick,PC-1050): Go over old impl a few more times. Any features I missed? Oversimplified? Etc.
        return renderer(value);
      },
      original: col,
      align: justify,
      ...gutterProps,
      sortType(a, b) {
        // TODO(nick,PC-1050): react-table inverts the return value for descent anyway. Remove third param.
        return sortFunc(a.original, b.original, true);
      },
    };
  };

  const columns = React.useMemo<ReactTable['columns']>(
    () => table.relation
      .getColumnsList()
      .map(convertColumn)
      .sort((a, b) => Number(b.isGutter) - Number(a.isGutter)),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [displayMap]);

  return React.useMemo(() => ({ columns, data: rows }), [columns, rows]);
}

// This one has a sidebar for the currently-selected row, rather than placing it inline like the main data table does.
// It scrolls independently of the table to its left.
const useLiveDataTableStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    width: '100%',
    height: '100%',
    flexFlow: 'row nowrap',
    justifyContent: 'stretch',
    alignItems: 'stretch',
    overflow: 'hidden',
    position: 'relative',
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

export const MinimalLiveDataTable: React.FC<{ table: VizierTable }> = ({ table }) => {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any>|null) => setDetails(row), [setDetails]);

  return (
    <div className={classes.root}>
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
};

export interface LiveDataTableProps extends Pick<DataTableProps, 'onRowsRendered'> {
  table: VizierTable;
  propagatedArgs?: Arguments;
  gutterColumn?: string;
}

export const LiveDataTable: React.FC<LiveDataTableProps> = ({ table, ...options }) => {
  const classes = useLiveDataTableStyles();
  const reactTable = useConvertedTable(table, options.propagatedArgs, options.gutterColumn);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any>|null) => setDetails(row), [setDetails]);

  // Determine if we should render row details in a horizontal split or a vertical one based on available space
  const [splitMode, setSplitMode] = React.useState<'horizontal'|'vertical'>('vertical');
  const rootRef = React.useCallback((el: HTMLDivElement) => {
    // TODO(nick,PC-1050): Make this update when widgets move (will need to watch nearest AutoSizerContext probably?)
    setSplitMode((el?.offsetWidth < el?.offsetHeight) ? 'horizontal' : 'vertical');
  }, [setSplitMode]);

  return (
    <div ref={rootRef} className={buildClass(classes.root, splitMode === 'horizontal' && classes.rootHorizontal)}>
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
};
