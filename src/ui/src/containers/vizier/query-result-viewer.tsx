import './query-result-viewer.scss';

import clsx from 'clsx';
import ClusterContext from 'common/cluster-context';
import { Table } from 'common/vizier-grpc-client';
import {
    AutoSizedScrollableTable, AutoSizedScrollableTableProps, TableColumnInfo,
} from 'components/table/scrollable-table';
import { isEntityType, toEntityPathname, toSingleEntityPage } from 'containers/live/utils/live-view-params';
import numeral from 'numeral';
import * as React from 'react';
import { Link } from 'react-router-dom';
import { DataType, Relation, SemanticType, Status } from 'types/generated/vizier_pb';
import * as FormatData from 'utils/format-data';
import { ParseCompilerErrors } from 'utils/parse-compiler-errors';
import { dataFromProto } from 'utils/result-data-utils';

// Formats int64 data, the input type is a string because JS does not
// natively support 64-bit data.
function formatInt64Data(val: string): string {
  return numeral(val).format('0,0');
}

function formatData(colType: DataType, data): string {
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES.
  switch (colType) {
    case DataType.STRING:
      return data;
    case DataType.TIME64NS:
      return new Date(data).toLocaleString();
    case DataType.DURATION64NS:
      return formatInt64Data(data);
    case DataType.INT64:
      return formatInt64Data(data);
    case DataType.UINT128:
      return data;
    case DataType.FLOAT64:
      return FormatData.formatFloat64Data(data);
    case DataType.BOOLEAN:
      return data ? 'true' : 'false';
    default:
      throw (new Error('Unknown data type: ' + colType));
  }
}

function computeColumnWidthRatios(relation: Relation, parsedTable: any): any {
  // Compute the average data width of a column (by name).
  const aveColWidth = {};
  let totalWidth = 0;
  relation.getColumnsList().forEach((col) => {
    const colName = col.getColumnName();
    aveColWidth[colName] = parsedTable.reduce((acc, val) => (
      acc + (val[colName].length / parsedTable.length)), 0);
    totalWidth += aveColWidth[colName];
  });

  const colWidthRatio = {};
  relation.getColumnsList().forEach((col) => {
    const colName = col.getColumnName();
    colWidthRatio[colName] = aveColWidth[colName] / totalWidth;
  });

  return colWidthRatio;
}

function toEntityLink(entity: string, semanticType: SemanticType, clusterName: string) {
  const page = toSingleEntityPage(entity, semanticType, clusterName);
  const pathname = toEntityPathname(page);
  return <Link to={pathname} className={'query-results--entity-link'}>{entity}</Link>;
}

function ResultCellRenderer(cellData: any, columnInfo: TableColumnInfo) {
  const dataType = columnInfo.dataType;
  const colName = columnInfo.label;

  if (isEntityType(columnInfo.semanticType)) {
    // Hack to handle cases like "['pl/service1', 'pl/service2']" which show up for pods that are part of 2 services.
    if (columnInfo.semanticType === SemanticType.ST_SERVICE_NAME) {
      try {
        const parsedArray = JSON.parse(cellData);
        if (Array.isArray(parsedArray)) {
          return (
            <>
              {
                parsedArray.map((entity, i) => {
                  return (
                    <span key={i}>
                      {i > 0 && ', '}
                      {toEntityLink(entity, columnInfo.semanticType, columnInfo.clusterName)}
                    </span>
                  );
                })
              }
            </>
          );
        }
      } catch (e) {
        //
      }
    }
    return toEntityLink(cellData, columnInfo.semanticType, columnInfo.clusterName);
  }

  const data = formatData(dataType, cellData);

  if (FormatData.looksLikeLatencyCol(colName, dataType)) {
    return FormatData.LatencyData(data);
  }

  if (FormatData.looksLikeAlertCol(colName, dataType)) {
    return FormatData.AlertData(data);
  }

  if (dataType !== DataType.STRING) {
    return data;
  }

  try {
    const jsonObj = JSON.parse(cellData);
    return <FormatData.JSONData
      data={jsonObj}
    />;
  } catch {
    return data;
  }
}

function ExpandedRowRenderer(rowData) {
  return <FormatData.JSONData
    className='query-results-expanded-row'
    data={rowData}
    multiline={true}
  />;
}

export const QueryResultErrors: React.FC<{ status: Status }> = ({ status }) => {
  const parsedErrors = ParseCompilerErrors(status);
  const colInfo: TableColumnInfo[] = [
    {
      dataKey: 'line',
      label: 'Line',
      dataType: DataType.INT64,
      semanticType: SemanticType.ST_UNSPECIFIED,
      flexGrow: 8,
      width: 10,
    }, {
      dataKey: 'column',
      label: 'Column',
      dataType: DataType.INT64,
      semanticType: SemanticType.ST_UNSPECIFIED,
      flexGrow: 8,
      width: 10,
    }, {
      dataKey: 'message',
      label: 'Message',
      dataType: DataType.STRING,
      semanticType: SemanticType.ST_UNSPECIFIED,
      flexGrow: 8,
      width: 600,
    },
  ];

  return (
    <div className='query-results--compiler-error'>
      <AutoSizedScrollableTable
        data={parsedErrors}
        columnInfo={colInfo}
        cellRenderer={ResultCellRenderer}
        expandable={true}
        expandRenderer={ExpandedRowRenderer}
        resizableCols={false}
      />
    </div>);
};

function parseTable(table: Table, clusterName: string): AutoSizedScrollableTableProps {
  const parsedTable = dataFromProto(table.relation, table.data);
  const colWidthRatio = computeColumnWidthRatios(table.relation, parsedTable);

  // TODO(zasgar/michelle): Clean this up and make sure it's consistent with the
  // CSS.
  const colWidth = 600;
  const minColWidth = 200;
  const columnInfo: TableColumnInfo[] = table.relation.getColumnsList().map((col) => {
    const colName = col.getColumnName();
    return {
      dataKey: colName,
      label: colName,
      clusterName: clusterName,
      dataType: col.getColumnType(),
      semanticType: col.getColumnSemanticType(),
      flexGrow: 8,
      width: Math.max(minColWidth, colWidthRatio[colName] * colWidth),
    };
  });
  return {
    data: parsedTable,
    columnInfo,
    cellRenderer: ResultCellRenderer,
    expandable: true,
    expandRenderer: ExpandedRowRenderer,
    resizableCols: true,
  };
}

export interface QueryResultTableProps {
  data: Table;
  className?: string;
}

export const QueryResultTable = React.memo<QueryResultTableProps>(({ data, className }) => {
  const { selectedClusterName } = React.useContext(ClusterContext);
  const props = parseTable(data, selectedClusterName);
  return (
    <div className={clsx('query-results', className)}>
      <AutoSizedScrollableTable
        {...props}
      />
    </div>
  );
});

QueryResultTable.displayName = 'QueryResultTable';
