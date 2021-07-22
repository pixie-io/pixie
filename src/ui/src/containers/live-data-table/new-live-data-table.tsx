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
import { dataFromProto } from 'app/utils/result-data-utils';
import { ReactTable, DataTable } from 'app/components/data-table/new-data-table';
import { DataType, Relation, SemanticType } from 'app/types/generated/vizierapi_pb';
import { CellAlignment } from 'app/components';
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
function useConvertedTable(table: VizierTable): ReactTable {
  // Some cell renderers need a bit of extra information that isn't directly related to the table.
  const theme = useTheme();
  const { selectedClusterName: clusterName } = React.useContext(ClusterContext);
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

    const renderer = liveCellRenderer(display, updateDisplay, true, theme, clusterName, rows, embedState);

    const sortFunc = getSortFunc(display);

    return {
      Header: titleFromInfo(display),
      accessor: col.getColumnName(),
      Cell({ value }) {
        // TODO(nick,PC-1050): Quantile columns are slow (even resizing/scrolling). Also true for old impl. Bad memo?
        // TODO(nick,PC-1050): Gutter/ctrl cell support (outside of the drawer). They need a prop for "no right border".
        // TODO(nick,PC-1050): We're not doing width weights yet. Need to. Convert to ratio of default in DataTable?
        // TODO(nick,PC-1050): Head/tail mode (data-table.tsx) for not-the-data-drawer.
        // TODO(nick,PC-1050): Go over old impl a few more times. Any features I missed? Oversimplified? Etc.
        return renderer(value);
      },
      original: col,
      align: justify,
      sortType(a, b) {
        // TODO(nick,PC-1050): react-table inverts the return value for descent anyway. Remove third param.
        return sortFunc(a.original, b.original, true);
      },
    };
  };

  const columns = React.useMemo<ReactTable['columns']>(
    () => table.relation.getColumnsList().map(convertColumn),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [displayMap]);

  return React.useMemo(() => ({ columns, data: rows }), [columns, rows]);
}

// This one has a sidebar for the currently-selected row, rather than placing it inline like the main data table does.
// It scrolls independently of the table to its left.
const useMinimalTableStyles = makeStyles((theme: Theme) => createStyles({
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
  table: {
    flex: 3,
    overflow: 'hidden',
    width: '100%', // It's using an AutoSizer that has a natural width/height of 0. Need to force it to use space.
  },
  details: {
    flex: 1,
    minWidth: '0px',
    whiteSpace: 'pre-wrap',
    overflow: 'auto',
    borderLeft: `1px solid ${theme.palette.background.three}`,
    padding: theme.spacing(2),

    '& *': { whiteSpace: 'pre-wrap' },
  },
}), { name: 'MinimalLiveDataTable' });

export const MinimalLiveDataTable: React.FC<{ table: VizierTable, elevation?: number }> = ({ table, elevation }) => {
  const classes = useMinimalTableStyles();
  const reactTable = useConvertedTable(table);

  const [details, setDetails] = React.useState<Record<string, any>>(null);
  const onRowSelected = React.useCallback((row: Record<string, any>|null) => setDetails(row), [setDetails]);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <DataTable table={reactTable} enableRowSelect onRowSelected={onRowSelected} elevation={elevation} />
      </div>
      {details && ( // TODO: Despite white-space:pre-wrap, this doesn't do so. Old impl does. Difference?
        <div className={classes.details}>
          <JSONData data={details} multiline />
        </div>
      )}
    </div>
  );
};
