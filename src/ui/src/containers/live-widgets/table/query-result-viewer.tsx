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

import { WidgetDisplay } from 'app/containers/live/vis';
import { ROW_RETENTION_LIMIT, VizierTable } from 'app/api';
import * as React from 'react';
import { alpha } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { Arguments } from 'app/utils/args-utils';
import { LiveDataTable } from 'app/containers/live-data-table/live-data-table';
import { ResultsContext, useLatestRowCount } from 'app/context/results-context';

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
  overload: {
    fontStyle: 'italic',
    color: alpha(palette.foreground.one, 0.8),
  },
}), { name: 'QueryResultViewer' });

export interface QueryResultTableDisplay extends WidgetDisplay {
  gutterColumn?: string,
}

export interface QueryResultTableProps {
  display: QueryResultTableDisplay;
  table: VizierTable;
  propagatedArgs: Arguments;
}

export const QueryResultTable = React.memo<QueryResultTableProps>(function QueryResultTable({
  display, table, propagatedArgs,
}) {
  const classes = useStyles();
  const { streaming } = React.useContext(ResultsContext);

  // Ensures the summary updates while streaming queries.
  const numRows = useLatestRowCount(table.name);
  const showOverloadWarning = streaming && numRows >= ROW_RETENTION_LIMIT;

  const [visibleStart, setVisibleStart] = React.useState(1);
  const [visibleStop, setVisibleStop] = React.useState(1);
  const visibleRowSummary = React.useMemo(() => {
    const count = visibleStop - visibleStart + 1;
    let text = `Showing ${visibleStart + 1} - ${visibleStop + 1} / ${numRows} records`;
    if (count <= 0) {
      text = 'No records to show';
    } else if (count >= numRows) {
      text = '\xa0'; // non-breaking space
    }
    return text;
  }, [numRows, visibleStart, visibleStop]);

  const onRowsRendered = React.useCallback(({ visibleStartIndex, visibleStopIndex }) => {
    setVisibleStart(visibleStartIndex);
    setVisibleStop(visibleStopIndex);
  }, []);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <LiveDataTable
          table={table}
          gutterColumn={display.gutterColumn}
          propagatedArgs={propagatedArgs}
          onRowsRendered={onRowsRendered}
        />
      </div>
      <div className={classes.tableSummary}>
        <span>{visibleRowSummary}</span>
        {showOverloadWarning && (
          <span className={classes.overload}>{' (keeping only latest to reduce memory pressure)'}</span>
        )}
      </div>
    </div>
  );
});
