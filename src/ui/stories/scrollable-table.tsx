import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { AutoSizedScrollableTable } from '../src/components/table/scrollable-table';

function ExpandRenderer(rowData) {
  return (
    <div>{`This is an expanded form of the row containing data for ${JSON.stringify(rowData)}`}</div>
  );
}

storiesOf('ScrollableTable', module)
  .add('Basic', () => (
    <div style={{ height: 150 }}>
      <AutoSizedScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
      />
    </div>
  ), {
    info: { inline: true },
    notes: 'This is a regular button that will be used in our UI.',
  }).add('Expandable', () => (
    <div style={{ height: 150 }}>
      <AutoSizedScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        expandable
        expandRenderer={ExpandRenderer}
      />
    </div>
  ), {
    info: { inline: true },
    notes: 'This is a regular button that will be used in our UI.',
  }).add('Resizable columns', () => (
    <div style={{ height: 150 }}>
      <AutoSizedScrollableTable
        data={[
          { col1: 1, col2: 'this is a string', col3: 100 },
          { col1: 2, col2: 'hello', col3: 200 },
          { col1: 3, col2: 'world', col3: 300 },
        ]}
        columnInfo={[
          { dataKey: 'col1', label: 'Col1', width: 60 },
          { dataKey: 'col2', label: 'Col2', width: 100 },
          { dataKey: 'col3', label: 'Col3', width: 60 },
        ]}
        expandable
        expandRenderer={ExpandRenderer}
        resizableCols
      />
    </div>
  ), {
    info: { inline: true },
    notes: 'This is a table with resizable columns.',
  });
