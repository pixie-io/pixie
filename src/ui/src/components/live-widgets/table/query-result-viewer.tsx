import clsx from 'clsx';
import ClusterContext from 'common/cluster-context';
import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';
import {
  createStyles, Theme, Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { IndexRange } from 'react-virtualized';
import { VizierDataTable } from '../../vizier-data-table/vizier-data-table';
import { JSONData } from '../../format-data/format-data';

const styles = ({ spacing }: Theme) => createStyles({
  root: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    '@global': {
      '.ReactVirtualized__Table__row': {
        fontSize: '0.8rem',
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

export interface QueryResultTableProps extends WithStyles<typeof styles> {
  data: Table;
}

const QueryResultTableBare = (({ data, classes }: QueryResultTableProps) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const ExpandedRowRenderer = (rowData: any) => (
    <JSONData
      data={rowData}
      multiline
    />
  );
  const [count, setCount] = React.useState<number>(0);
  const [totalCount, setTotalCount] = React.useState<number>(0);

  React.useEffect(() => {
    if (data && data.data) {
      setTotalCount(
        data.data.map((d) => d.getNumRows())
          .reduce((p, n) => p + n, 0));
    }
  }, [data, setTotalCount]);

  const getTableSummary = React.useCallback(() => {
    let summary = `Showing ${count} of ${totalCount} records`;
    if (count <= 0) {
      summary = 'No records to show';
    }
    return <Typography variant='subtitle2'>{summary}</Typography>;
  }, [count, totalCount]);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <VizierDataTable
          table={data}
          expandable
          expandedRenderer={ExpandedRowRenderer}
          prettyRender
          clusterName={selectedClusterName}
          onRowsRendered={(info: IndexRange) => {
            setCount(info.stopIndex - info.startIndex + 1);
          }}
        />
      </div>
      <div className={classes.tableSummary}>
        {getTableSummary()}
      </div>
    </div>
  );
});

export const QueryResultTable = withStyles(styles)(QueryResultTableBare);
