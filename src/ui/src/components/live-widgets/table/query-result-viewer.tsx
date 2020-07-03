import './query-result-viewer.scss';
import clsx from 'clsx';
import ClusterContext from 'common/cluster-context';
import { Table } from 'common/vizier-grpc-client';
import * as React from 'react';
import { VizierDataTable } from '../../vizier-data-table/vizier-data-table';
import { JSONData } from '../../format-data/format-data';

export interface QueryResultTableProps {
  data: Table;
  className?: string;
}

export const QueryResultTable = (({ data, className }: QueryResultTableProps) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const ExpandedRowRenderer = (rowData: any) => (
    <JSONData
      className='query-results-expanded-row'
      data={rowData}
      multiline
    />
  );

  return (
    <div className={clsx('query-results', className)}>
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
