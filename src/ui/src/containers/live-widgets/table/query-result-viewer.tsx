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

import { alpha } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { ROW_RETENTION_LIMIT, VizierTable } from 'app/api';
import { CompleteColumnDef, LiveDataTable } from 'app/containers/live-data-table/live-data-table';
import { WidgetDisplay } from 'app/containers/live/vis';
import { ResultsContext, useLatestRowCount } from 'app/context/results-context';
import { Arguments } from 'app/utils/args-utils';

const useStyles = makeStyles(({ spacing, typography, palette }: Theme) => createStyles({
  root: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    overflowX: 'hidden',
  },
  table: {
    display: 'flex',
    flexGrow: 1,
  },
  tableSummary: {
    marginTop: spacing(1.5),
    marginBottom: spacing(0.5),
    paddingTop: spacing(1),
    paddingRight: spacing(1),
    textAlign: 'right',
    ...typography.subtitle2,
  },
  tableSummaryExtern: { // For when this is sent up to a widget titlebar
    ...typography.caption,
    color: palette.foreground.one,

    '& $overload': { color: palette.foreground.three },
    '& $muted': { color: palette.foreground.three },
  },
  overload: {
    fontStyle: 'italic',
    color: alpha(palette.foreground.one, 0.8),
  },
  muted: {
    color: alpha(palette.foreground.one, 0.8),
  },
}), { name: 'QueryResultViewer' });

const TableSummary = React.memo<{
  visibleStart: number, visibleStop: number, numRows: number, isOverload: boolean,
}>(({
  visibleStart, visibleStop, numRows, isOverload,
}) => {
  const classes = useStyles();

  const overloadWarning = isOverload
    ? <span className={classes.overload}>{' (keeping only latest to reduce memory pressure)'}</span>
    : '';

  const count = visibleStop - visibleStart + 1;

  if (count <= 0) {
    return <span>No records to show</span>;
  } else if (count >= numRows) {
    return <>
      <span>Showing {count} records</span>
      {overloadWarning}
    </>;
  } else {
    return (
      <span>
        Showing {visibleStart + 1} - {visibleStop + 1}
        <span className={classes.muted}>{' out of '}</span>
        {numRows} records
        {overloadWarning}
      </span>
    );
  }
});
TableSummary.displayName = 'TableSummary';

export interface QueryResultTableDisplay extends WidgetDisplay {
  gutterColumn?: string,
}

export interface QueryResultTableProps {
  display: QueryResultTableDisplay;
  table: VizierTable;
  propagatedArgs: Arguments;
  customGutters?: Array<CompleteColumnDef>;
  /** If set, controls including the table summary will be rendered to this ref instead of underneath the table */
  setExternalControls?: React.RefCallback<React.ReactNode>;
}

export const QueryResultTable = React.memo<QueryResultTableProps>(({
  display, table, propagatedArgs, customGutters = [], setExternalControls,
}) => {
  const classes = useStyles();
  const { streaming } = React.useContext(ResultsContext);

  // Ensures the summary updates while streaming queries.
  const numRows = useLatestRowCount(table.name);
  const isOverload = streaming && numRows >= ROW_RETENTION_LIMIT;

  const [visibleStart, setVisibleStart] = React.useState(1);
  const [visibleStop, setVisibleStop] = React.useState(1);

  const onRowsRendered = React.useCallback(({ visibleStartIndex, visibleStopIndex }) => {
    setVisibleStart(visibleStartIndex);
    setVisibleStop(visibleStopIndex);
  }, []);

  React.useEffect(() => {
    if (setExternalControls) {
      setExternalControls(
        <div className={classes.tableSummaryExtern}>
          <TableSummary
            visibleStart={visibleStart}
            visibleStop={visibleStop}
            numRows={numRows}
            isOverload={isOverload}
          />
        </div>,
      );
    }
  }, [setExternalControls, isOverload, numRows, visibleStart, visibleStop, classes.tableSummaryExtern]);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <LiveDataTable
          table={table}
          gutterColumns={[display.gutterColumn, ...customGutters].filter(g => g)}
          propagatedArgs={propagatedArgs}
          onRowsRendered={onRowsRendered}
        />
      </div>
      {!setExternalControls && (
        <div className={classes.tableSummary}>
          <TableSummary
            visibleStart={visibleStart}
            visibleStop={visibleStop}
            numRows={numRows}
            isOverload={isOverload}
          />
        </div>
      )}
    </div>
  );
});
QueryResultTable.displayName = 'QueryResultTable';
