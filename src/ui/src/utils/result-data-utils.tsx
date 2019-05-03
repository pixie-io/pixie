import * as _ from 'lodash';

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
          data = data.replace(/"/g, '\\\"');
        }
        rowData.push('"' + data + '"');
      }
      csvStr += rowData.join() + '\n';
    }
  });

  return csvStr;
}
