import clsx from 'clsx';
import ClusterContext from 'common/cluster-context';
import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';
import {
  createStyles,
  withStyles,
  WithStyles,
} from '@material-ui/core';
import { VizierDataTable } from '../../vizier-data-table/vizier-data-table';
import { JSONData } from '../../format-data/format-data';

const styles = () => createStyles({
  root: {
    height: '100%',
    flex: 'unset',
    display: 'flex',
    flexDirection: 'column',
    '@global': {
      '.ReactVirtualized__Table__row': {
        fontSize: '0.8rem',
      },
    },
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

  return (
    <div className={classes.root}>
      <VizierDataTable
        table={data}
        expandable
        expandedRenderer={ExpandedRowRenderer}
        prettyRender
        clusterName={selectedClusterName}
      />
    </div>
  );
});

export const QueryResultTable = withStyles(styles)(QueryResultTableBare);
