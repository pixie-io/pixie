import { AutoSizedScrollableTable, TableColumnInfo } from 'components/table/scrollable-table';
import * as React from 'react';
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

function looksLikeLatencyCol(colName: string, colType: string) {
  if (colType !== 'FLOAT64') {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/latency.*/)) {
    return true;
  }
  if (colNameLC.match(/p\d{0,2}$/)) {
    return true;
  }
}

function looksLikeAlertCol(colName: string, colType: string) {
  if (colType !== 'BOOLEAN') {
    return false;
  }
  const colNameLC = colName.toLowerCase();
  if (colNameLC.match(/alert.*/)) {
    return true;
  }
}

function formatAlertCol(colName: string, parsedTable: any) {
  for (const row of parsedTable) {
    let val = row[colName];
    if (val === 'true') {
      val = '<div class=\'col_data--alert-true\'>' + val + '</div>';
    } else {
      val = '<div class=\'col_data--alert-false\'>' + val + '</div>';
    }
    row[colName] = val;
  }
}

function formatLatencyCol(colName: string, parsedTable: any) {
  for (const row of parsedTable) {
    let val = row[colName];
    // TODO(zasgar/michelle): We should move this up so we don't have to
    // decode the float here.
    const floatVal = parseFloat(val);
    if (floatVal > 300) {
      val = '<div class=\'col_data--latency-high\'>' + val + '</div>';
    } else if (floatVal > 150) {
      val = '<div class=\'col_data--latency-med\'>' + val + '</div>';
    } else {
      val = '<div class=\'col_data--latency-low\'>' + val + '</div>';
    }
    row[colName] = val;
  }
}

function prettyPrintJSON(jsonObj: object) {
  let jsonStr: string = JSON.stringify(jsonObj, null, 2);
  jsonStr = jsonStr.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  return jsonStr.replace(
    /("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g,
    (match) => {
      let cls = 'col_data--json-number';
      if (/^"/.test(match)) {
          if (/:$/.test(match)) {
              cls = 'col_data--json-key';
          } else {
              cls = 'col_data--json-string';
          }
      } else if (/true|false/.test(match)) {
          cls = 'col_data--json-boolean';
      } else if (/null/.test(match)) {
          cls = 'col_data--json-null';
      }
      return '<span class="' + cls + '">' + match + '</span>';
  });
}

function prettyFormatString(colName: string, parsedTable: any) {
  for (const row of parsedTable) {
    const val = row[colName];
    try {
      const jsonObj = JSON.parse(val);
      row[colName] = prettyPrintJSON(jsonObj);
    } catch {
      // Ingore, string won't be formatted.
    }
  }
}

// Formats the data by applying in place edits.
function formatData(relation: GQLDataTableRelation, parsedTable) {
  relation.colNames.forEach((colName, colIdx) => {
    const colType = relation.colTypes[colIdx];
    if (looksLikeLatencyCol(colName, colType)) {
      formatLatencyCol(colName, parsedTable);
      return;
    }

    if (looksLikeAlertCol(colName, colType)) {
      formatAlertCol(colName, parsedTable);
      return;
    }

    // Try to parse string as JSON if possible.
    prettyFormatString(colName, parsedTable);
  });
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
    const colInfo: TableColumnInfo[] = relation.colNames.map((colName) => {
      return {
        dataKey: colName,
        label: colName,
        flexGrow: 8,
        width: Math.max(minColWidth, colWidthRatio[colName] * colWidth),
      };
    });

    // TODO(zasgar/michelle): Uncomment this after we figure out how to
    // get react-virtualized to render styles.
    formatData(relation, parsedTable);

    return (
      <div className='query-results'>
        QueryID: {data.id}
        <br />
        <AutoSizedScrollableTable data={parsedTable} columnInfo={colInfo}/>
      </div>
    );
  }
}
