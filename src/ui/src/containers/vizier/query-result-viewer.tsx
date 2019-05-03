import { AutoSizedScrollableTable, TableColumnInfo } from 'components/table/scrollable-table';
import * as React from 'react';
import * as FormatData from 'utils/format-data';
// TODO(zasgar/michelle): Figure out how to impor schema properly
import {
  GQLDataColTypes,
  GQLDataTable,
  GQLDataTableRelation,
  GQLQuery,
  GQLQueryResult,
} from '../../../../services/api/controller/schema/schema';

import './vizier.scss';

export interface QueryResultViewerProps {
  data: GQLQueryResult;
}

function extractData(colType: string, col: any, rowIdx): string {
  switch (colType) {
    case 'STRING':
      return col.stringData.data[rowIdx];
    case 'TIME64NS':
      // Time is stored as a float b/c proto JSON
      // so we can easily just divide by 1000 and convert to time.
      const data = col.time64nsData.data[rowIdx];
      return new Date(parseFloat(data) / 1000000).toLocaleString();
    case 'INT64':
      return '' + col.int64Data.data[rowIdx];
    case 'FLOAT64':
      return col.float64Data.data[rowIdx].toFixed(2);
    case 'BOOLEAN':
       return col.booleanData.data[rowIdx] ? 'true' : 'false';
    default:
      throw(new Error('Unknown data type: ' + colType));
  }
}

// This function translates the incoming table into a array of object,
// where each key of the object is the column name according to the relation.
function parseDataTable(relation: GQLDataTableRelation, tableData): any {
  // The data is stored in columnar format, this converts
  // it to rows.
  const outputData = [];
  tableData.rowBatches.forEach((rowBatch) => {
    const numRows = rowBatch.numRows;
    for (let rowIdx = 0; rowIdx < numRows; rowIdx++) {
      const row = {};
      for (let colIdx = 0; colIdx < rowBatch.cols.length; colIdx++) {
        const colName = relation.colNames[colIdx];
        row[colName] = extractData(relation.colTypes[colIdx], rowBatch.cols[colIdx], rowIdx);
      }
      outputData.push(row);
    }
  });
  return outputData;
}

function computeColumnWidthRatios(relation: GQLDataTableRelation, parsedTable: any): any {
    // Compute the average data width of a column (by name).
    const aveColWidth = {};
    let totalWidth = 0;
    relation.colNames.forEach((colName) => {
      aveColWidth[colName] = parsedTable.reduce((acc, val) => (
        acc + (val[colName].length / parsedTable.length)), 0);
      totalWidth += aveColWidth[colName];
    });

    const colWidthRatio = {};
    relation.colNames.forEach((colName) => {
      colWidthRatio[colName] = aveColWidth[colName] / totalWidth;
    });

    return colWidthRatio;
}

function ResultCellRenderer(cellData: any, columnInfo: TableColumnInfo) {
  const colType = columnInfo.type;
  const colName = columnInfo.label;
  if (FormatData.looksLikeLatencyCol(colName, colType)) {
    return FormatData.LatencyData(cellData);
  }

  if (FormatData.looksLikeAlertCol(colName, colType)) {
    return FormatData.AlertData(cellData);
  }

  try {
    const jsonObj = JSON.parse(cellData);
    return <FormatData.JSONData
      data={jsonObj}
    />;
  } catch {
    return cellData;
  }
}

function ExpandedRowRenderer(rowData) {
  return <FormatData.JSONData
    data={rowData}
    multiline={true}
  />;
}

export class QueryResultViewer extends React.Component<QueryResultViewerProps, {}> {
  constructor(props) {
    super(props);
    this.state = {};
  }

  render() {
    const data = this.props.data;
    if (!data) {
      return <div>No Data Available</div>;
    }

    const relation = data.table.relation;
    const tableData = JSON.parse(data.table.data);
    const parsedTable = parseDataTable(relation, tableData);
    const colWidthRatio = computeColumnWidthRatios(relation, parsedTable);

    // TODO(zasgar/michelle): Clean this up and make sure it's consistent with the
    // CSS.
    const colWidth = 600;
    const minColWidth = 200;
    const colInfo: TableColumnInfo[] = relation.colNames.map((colName, idx) => {
      return {
        dataKey: colName,
        label: colName,
        type: relation.colTypes[idx],
        flexGrow: 8,
        width: Math.max(minColWidth, colWidthRatio[colName] * colWidth),
      };
    });

    return (
      <div className='query-results'>
        <AutoSizedScrollableTable
          data={parsedTable}
          columnInfo={colInfo}
          cellRenderer={ResultCellRenderer}
          expandable={true}
          expandRenderer={ExpandedRowRenderer}
        />
      </div>
    );
  }
}
