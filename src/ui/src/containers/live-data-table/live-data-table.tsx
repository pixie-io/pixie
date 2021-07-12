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

import { ClusterContext } from 'app/common/cluster-context';
import {
  CellAlignment, ColumnProps, DataTable, SortState,
  buildClass,
} from 'app/components';
import { JSONData } from 'app/containers/format-data/format-data';
import { STATUS_TYPES } from 'app/containers/live-widgets/utils';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import * as React from 'react';
import { Table } from 'app/api';
import { DataType, SemanticType } from 'app/types/generated/vizierapi_pb';
import noop from 'app/utils/noop';
import { dataFromProto } from 'app/utils/result-data-utils';
import {
  makeStyles,
  useTheme, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import { IndexRange } from 'react-virtualized';
import { Arguments } from 'app/utils/args-utils';

import { ColumnDisplayInfo, displayInfoFromColumn, titleFromInfo } from './column-display-info';
import { parseRows } from './parsers';
import { liveCellRenderer } from './renderers';
import { getSortFunc } from './sort-funcs';

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

// For certain semantic types, override the column width ratio based off of the rendering
// we expect to do for that semantic type.
const SemanticTypeWidthOverrideMap = new Map<SemanticType, number>(
  [
    [SemanticType.ST_QUANTILES, 40],
    [SemanticType.ST_DURATION_NS, 20],
    [SemanticType.ST_DURATION_NS_QUANTILES, 40],
  ],
);
const DataTypeWidthOverrideMap = new Map<DataType, number>(
  [
    [DataType.TIME64NS, 25],
  ],
);

function hasWidthOverride(st: SemanticType, dt: DataType): boolean {
  return SemanticTypeWidthOverrideMap.has(st) || DataTypeWidthOverrideMap.has(dt);
}

function getWidthOverride(st: SemanticType, dt: DataType): number {
  if (SemanticTypeWidthOverrideMap.has(st)) {
    return SemanticTypeWidthOverrideMap.get(st);
  }
  return DataTypeWidthOverrideMap.get(dt);
}

interface LiveDataTableProps {
  table: Table;
  prettyRender?: boolean;
  expandable?: boolean;
  expandedRenderer?: (rowIndex: number) => JSX.Element;
  clusterName?: string;
  gutterColumn?: string;
  onRowSelectionChanged?: (row: any) => void;
  onRowsRendered?: (range: IndexRange) => void;
  propagatedArgs?: Arguments;
}

export const LiveDataTable: React.FC<LiveDataTableProps> = (props) => {
  const {
    table, prettyRender = false, expandable = false, expandedRenderer,
    clusterName = null,
    gutterColumn = null,
    onRowSelectionChanged = noop,
    onRowsRendered = () => { },
    propagatedArgs = null,
  } = props;

  const { embedState } = React.useContext(LiveRouteContext);

  const [rows, setRows] = React.useState([]);
  const [selectedRow, setSelectedRow] = React.useState(-1);
  const [columnDisplayInfos, setColumnDisplayInfos] = React.useState<Map<string, ColumnDisplayInfo>>(
    new Map<string, ColumnDisplayInfo>());

  const dataLength = table.data ? table.data.length : 0;
  React.useEffect(() => {
    // Map containing the display information for the column.
    const displayInfos = new Map<string, ColumnDisplayInfo>();

    table.relation.getColumnsList().forEach((col) => {
      const name = col.getColumnName();
      const displayInfo = displayInfoFromColumn(col);
      displayInfos.set(name, displayInfo);
    });

    const semanticTypeMap = [...displayInfos.values()].reduce((acc, val) => {
      acc.set(val.columnName, val.semanticType);
      return acc;
    }, new Map<string, SemanticType>());

    const rawRows = dataFromProto(table.relation, table.data);
    if (prettyRender) {
      const parsedRows = parseRows(semanticTypeMap, rawRows);
      setRows(parsedRows);
    } else {
      setRows(rawRows);
    }
    setColumnDisplayInfos(displayInfos);
  }, [table, dataLength, clusterName, prettyRender]);

  const theme = useTheme();
  const [dataTableCols, gutterCol] = React.useMemo(() => {
    const allInfos = [...columnDisplayInfos.values()];

    const gutterInfo = allInfos.find((displayInfo: ColumnDisplayInfo) => (
      displayInfo.columnName === gutterColumn && STATUS_TYPES.has(displayInfo.semanticType)
    ));

    const remainingInfos = allInfos.filter((displayInfo: ColumnDisplayInfo) => (
      displayInfo.columnName !== gutterInfo?.columnName
    ));

    const displayInfoToProps = (displayInfo: ColumnDisplayInfo) => {
      if (!displayInfo) {
        return null;
      }
      // Some cells give the power to update the display state for the whole column.
      // This function is the handle that allows them to do that.
      const updateColumnDisplay = ((newColumnDisplay: ColumnDisplayInfo) => {
        const newMap = new Map<string, ColumnDisplayInfo>(
          columnDisplayInfos.set(displayInfo.columnName, newColumnDisplay));
        setColumnDisplayInfos(newMap);
      });

      const colProps: ColumnProps = {
        dataKey: displayInfo.columnName,
        label: titleFromInfo(displayInfo),
        align: SemanticAlignmentMap.get(displayInfo.semanticType) ?? DataAlignmentMap.get(displayInfo.type) ?? 'start',
        cellRenderer: liveCellRenderer(displayInfo, updateColumnDisplay, prettyRender,
          theme, clusterName, rows, embedState, propagatedArgs),
      };
      if (hasWidthOverride(displayInfo.semanticType, displayInfo.type)) {
        colProps.width = getWidthOverride(displayInfo.semanticType, displayInfo.type);
      }
      return colProps;
    };

    return [[...remainingInfos].map(displayInfoToProps), displayInfoToProps(gutterInfo)];
  }, [columnDisplayInfos, gutterColumn, clusterName, prettyRender, propagatedArgs, rows, theme]);

  const rowGetter = React.useCallback(
    (i) => rows[i],
    [rows],
  );

  const onSort = React.useCallback((sortState: SortState) => {
    const column = columnDisplayInfos.get(sortState.dataKey);
    setRows(rows.sort(getSortFunc(column, sortState.direction)));
    setSelectedRow(-1);
    onRowSelectionChanged(null);
  }, [rows, columnDisplayInfos, onRowSelectionChanged]);

  const onRowSelect = React.useCallback((rowIndex) => {
    let newRowIndex = rowIndex;
    if (rowIndex === selectedRow) {
      newRowIndex = -1;
    }
    setSelectedRow(newRowIndex);
    onRowSelectionChanged(rows[newRowIndex]);
  }, [rows, selectedRow, onRowSelectionChanged]);

  if (rows.length === 0) {
    return null;
  }

  return (
    <DataTable
      rowGetter={rowGetter}
      rowCount={rows.length}
      columns={dataTableCols}
      gutterColumn={gutterCol}
      compact
      onSort={onSort}
      onRowClick={onRowSelect}
      highlightedRow={selectedRow}
      expandable={expandable}
      expandedRenderer={expandedRenderer}
      onRowsRendered={onRowsRendered}
    />
  );
};

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    flexDirection: 'row',
    height: '100%',
    position: 'relative',
  },
  details: {
    flex: 1,
    padding: theme.spacing(2),
    borderLeft: `solid 1px ${theme.palette.background.three}`,
    minWidth: 0,
    overflow: 'auto',
    whiteSpace: 'pre-wrap',
  },
  table: {
    flex: 3,
    overflowX: 'hidden',
  },
  close: {
    position: 'absolute',
  },
}));

interface LiveDataRowDetailsProps {
  data?: any;
}

const LiveDataRowDetails: React.FC<LiveDataRowDetailsProps> = ({ data }) => {
  const classes = useStyles();
  if (!data) {
    return null;
  }
  return (
    <div className={classes.details}>
      <JSONData data={data} multiline />
    </div>
  );
};

export const LiveDataTableWithDetails: React.FC<{ table: Table }> = (props) => {
  const [details, setDetails] = React.useState(null);
  const { selectedClusterName } = React.useContext(ClusterContext);

  const onRowSelection = React.useCallback((row) => {
    setDetails(row);
  }, [setDetails]);

  const classes = useStyles();
  const dataTableClass = buildClass(
    'fs-exclude',
    classes.root,
  );

  return (
    <div className={dataTableClass}>
      <div className={classes.table}>
        <LiveDataTable
          prettyRender
          expandable={false}
          table={props.table}
          clusterName={selectedClusterName}
          onRowSelectionChanged={onRowSelection}
        />
      </div>
      <LiveDataRowDetails data={details} />
    </div>
  );
};
