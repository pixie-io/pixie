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
import { WidgetDisplay } from 'app/containers/live/vis';
import { Table } from 'app/api';
import * as React from 'react';
import {
  Theme, Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import { IndexRange } from 'react-virtualized';
import { Arguments } from 'app/utils/args-utils';
import { LiveDataTable } from 'app/containers/live-data-table/live-data-table';
import { JSONData } from 'app/containers/format-data/format-data';

const styles = ({ spacing }: Theme) => createStyles({
  root: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    overflowX: 'hidden',
    '@global': {
      '.ReactVirtualized__Table__row': {
        fontSize: '0.975rem',
      },
    },
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
  },
});

export interface QueryResultTableDisplay extends WidgetDisplay {
  gutterColumn?: string,
}

export interface QueryResultTableProps extends WithStyles<typeof styles> {
  display: QueryResultTableDisplay;
  data: Table;
  propagatedArgs: Arguments;
}

const QueryResultTableBare = (({
  display, data, classes, propagatedArgs,
}: QueryResultTableProps) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const ExpandedRowRenderer = (rowData: any) => (
    <JSONData
      data={rowData}
      multiline
    />
  );
  const [indexRange, setIndexRange] = React.useState<IndexRange>({ startIndex: 0, stopIndex: 0 });
  const [totalCount, setTotalCount] = React.useState<number>(0);

  const dataLength = data && data.data ? data.data.length : 0;
  React.useEffect(() => {
    if (data && data.data) {
      setTotalCount(
        data.data.map((d) => d.getNumRows())
          .reduce((p, n) => p + n, 0));
    }
  }, [data, dataLength, setTotalCount]);

  const getTableSummary = React.useCallback(() => {
    const start = indexRange.startIndex;
    const stop = indexRange.stopIndex;
    const count = stop - start + 1;
    let summary = `Showing ${start + 1} - ${stop + 1} / ${totalCount} records`;
    if (count <= 0) {
      summary = 'No records to show';
    } else if (count >= totalCount) {
      summary = '';
    }
    return <Typography variant='subtitle2'>{summary}</Typography>;
  }, [indexRange, totalCount]);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <LiveDataTable
          table={data}
          expandable
          expandedRenderer={ExpandedRowRenderer}
          prettyRender
          gutterColumn={display.gutterColumn}
          clusterName={selectedClusterName}
          onRowsRendered={(info: IndexRange) => {
            setIndexRange(info);
          }}
          propagatedArgs={propagatedArgs}
        />
      </div>
      <div className={classes.tableSummary}>
        {getTableSummary()}
      </div>
    </div>
  );
});

export const QueryResultTable = withStyles(styles)(QueryResultTableBare);
