import { ClusterContext } from 'common/cluster-context';
import { VizierTable as Table } from 'pixie-api';
import * as React from 'react';
import {
  createStyles, Theme, Typography,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { IndexRange } from 'react-virtualized';
import { Arguments } from 'utils/args-utils';
import { VizierDataTable } from '../../vizier-data-table/vizier-data-table';
import { JSONData } from '../../format-data/format-data';

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

export interface QueryResultTableProps extends WithStyles<typeof styles> {
  data: Table;
  propagatedArgs: Arguments;
}

const QueryResultTableBare = (({ data, classes, propagatedArgs }: QueryResultTableProps) => {
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
        <VizierDataTable
          table={data}
          expandable
          expandedRenderer={ExpandedRowRenderer}
          prettyRender
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
