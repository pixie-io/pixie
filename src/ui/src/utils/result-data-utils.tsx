import * as _ from 'lodash';
import {extractData} from '../components/chart/data';

export function ResultsToCsv(results) {
  const jsonResults = JSON.parse(results);
  let csvStr = '';

  csvStr += _.map(jsonResults.relation.columns, 'columnName').join() + '\n';
  _.each(jsonResults.rowBatches, (rowBatch) => {
    const numRows = parseInt(rowBatch.numRows, 10);
    const numCols = rowBatch.cols.length;
    for (let i = 0; i < numRows; i++) {
      const rowData = [];
      for (let j = 0; j < numCols; j++) {
        const colKey = Object.keys(rowBatch.cols[j])[0];
        let data = rowBatch.cols[j][colKey].data[i];
        if (typeof data === 'string') {
          data = data.replace(/"/g, '\\\\\"\"');
          data = data.replace(/^{/g, '""{');
          data = data.replace(/}$/g, '}""');
          data = data.replace(/^\[/g, '""[');
          data = data.replace(/\[$/g, ']""');
        }
        rowData.push('"' + data + '"');
      }
      csvStr += rowData.join() + '\n';
    }
  });

  return csvStr;
}

export function ResultsToJSON(results) {
    let resValues = [];

    for (const batch of results.rowBatches) {
      const formattedBatch = [];
      for (let i = 0; i < parseInt(batch.numRows, 10); i++) {
        formattedBatch.push({});
      }

      batch.cols.forEach((col, i) => {
        const type = results.relation.columns[i].columnType;
        const name = results.relation.columns[i].columnName;

        const extractedData = extractData(type, col);

        extractedData.forEach((d, j) => {
          formattedBatch[j][name] = d;
        });
      });
      resValues = resValues.concat(formattedBatch);
    }

    return resValues;
}
