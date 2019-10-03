import { AutoSizedScrollableTable, TableColumnInfo } from 'components/table/scrollable-table';
import { ParseCompilerErrors } from 'utils/parse-compiler-errors';
import * as numeral from 'numeral';
import * as React from 'react';
import * as FormatData from 'utils/format-data';

// TODO(zasgar/michelle): Figure out how to import schema properly
import {
  GQLDataColTypes,
  GQLDataTable,
  GQLDataTableRelation,
  GQLCompilerErrors,
  GQLQueryErrors,
  GQLQuery,
  GQLQueryResult,
} from '../../../../vizier/services/api/controller/schema/schema';

import './vizier.scss';

export interface QueryResultViewerProps {
  data: GQLQueryResult;
}

// TODO(zasgar/michelle): remove when we upgrade to TS 3.2.
declare function BigInt(string): any;

// Converts UInt128 to UUID formatted string.
function formatUInt128(high: string, low: string): string {
  // TODO(zasgar/michelle): Revisit this to check and make sure endianness is correct.
  // Each segment of the UUID is a hex value of 16 nibbles.
  // Note: BigInt support only available in Chrome > 67, FF > 68.
  const hexStrHigh = BigInt(high).toString(16).padStart(16, '0');
  const hexStrLow = BigInt(low).toString(16).padStart(16, '0');

  // Sample UUID: 123e4567-e89b-12d3-a456-426655440000.
  // Format is 8-4-4-4-12.
  let uuidStr = '';
  uuidStr += hexStrHigh.substr(0, 8);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(8, 4);
  uuidStr += '-';
  uuidStr += hexStrHigh.substr(12, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(0, 4);
  uuidStr += '-';
  uuidStr += hexStrLow.substr(4);
  return uuidStr;
}

// Formats int64 data, the input type is a string because JS does not
// natively support 64-bit data.
function formatInt64Data(val: string): string {
  return numeral(val).format('0,0');
}

function formatFloat64Data(val: number): string {
  return numeral(val).format('0[.]00');
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
      return formatInt64Data(col.int64Data.data[rowIdx]);
    case 'UINT128':
      const v = col.uint128Data.data[rowIdx];
      return formatUInt128(v.high, v.low);
    case 'FLOAT64':
      return formatFloat64Data(col.float64Data.data[rowIdx]);
    case 'BOOLEAN':
       return col.booleanData.data[rowIdx] ? 'true' : 'false';
    default:
      throw(new Error('Unknown data type: ' + colType));
  }
}

// This function translates the incoming table into a array of object,
// where each key of the object is the column name according to the relation.
function parseDataTable(relation: GQLDataTableRelation, tableData): any {
  if (!tableData || !Array.isArray(tableData.rowBatches)) {
    // No row batches available.
    return [];
  }

  // The data is stored in columnar format, this converts it to rows.
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

function formatError(error: GQLQueryErrors) {
    const parsedErrors = ParseCompilerErrors(error.compilerError);
    const colInfo: TableColumnInfo[] = [
      {
        dataKey: 'line',
        label: 'Line',
        type: 'INT64',
        flexGrow: 8,
        width: 10,
      }, {
        dataKey: 'col',
        label: 'Column',
        type: 'INT64',
        flexGrow: 8,
        width: 10,
      }, {
        dataKey: 'msg',
        label: 'Message',
        type: 'STRING',
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

    if (data.error.compilerError) {
      return formatError(data.error);
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
          resizableCols={true}
        />
      </div>
    );
  }
}
