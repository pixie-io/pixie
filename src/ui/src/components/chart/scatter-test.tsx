import {mount} from 'enzyme';
import * as React from 'react';
import {LineSeries, MarkSeries} from 'react-vis';
import {
    Column, DataType, Float64Column, Relation, RowBatchData, Time64NSColumn,
} from 'types/generated/vizier_pb';

import {parseData, ScatterPlot} from './scatter';

describe('parseData', () => {
  it('returns scatter plot data if the tables conforms to it', () => {
    const relationCols = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols[0].setColumnName('time');
    relationCols[0].setColumnType(DataType.TIME64NS);
    relationCols[1].setColumnName('float64');
    relationCols[1].setColumnType(DataType.FLOAT64);
    const relation = new Relation();
    relation.setColumnsList(relationCols);
    const rowBatch = new RowBatchData();
    const timeColumn = new Column();
    const timeData = new Time64NSColumn();
    timeData.setDataList([123456767]);
    timeColumn.setTime64nsData(timeData);
    const floatColumn = new Column();
    const floatData = new Float64Column();
    floatData.setDataList([123.456]);
    floatColumn.setFloat64Data(floatData);
    rowBatch.setColsList([timeColumn, floatColumn]);

    const data = [rowBatch];
    const table1 = {
      relation,
      data,
      name: 'table1',
      id: 'id1',
    };

    const relationCols2 = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols2[0].setColumnName('time');
    relationCols2[0].setColumnType(DataType.TIME64NS);
    relationCols2[1].setColumnName('float64');
    relationCols2[1].setColumnType(DataType.FLOAT64);
    const relation2 = new Relation();
    relation.setColumnsList(relationCols2);

    const data2 = [rowBatch];
    const table2 = {
      relation: relation2,
      data: data2,
      name: 'table2',
      id: 'id2',
    };
    expect(parseData([table1, table2])).not.toBeNull();
  });

  it('returns null if no scatter plot data', () => {
    const relationCols = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols[0].setColumnName('time');
    relationCols[0].setColumnType(DataType.TIME64NS);
    relationCols[1].setColumnName('float64');
    relationCols[1].setColumnType(DataType.FLOAT64);
    const relation = new Relation();
    relation.setColumnsList(relationCols);

    const data = [
      new RowBatchData(),
    ];
    const table1 = {
      relation,
      data,
      name: 'table1',
      id: 'id1',
    };

    const relationCols2 = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols2[0].setColumnName('time');
    relationCols2[0].setColumnType(DataType.TIME64NS);
    relationCols2[1].setColumnName('float64');
    relationCols2[1].setColumnType(DataType.FLOAT64);
    const relation2 = new Relation();
    relation.setColumnsList(relationCols2);

    const data2 = [
      new RowBatchData(),
    ];
    const table2 = {
      relation: relation2,
      data: data2,
      name: 'table2',
      id: 'id2',
    };
    expect(parseData([table1, table2])).toBeNull();
  });
});

describe('<ScatterPlot> component', () => {
  const scatterPlotPoints = [
    {
      x: new Date('2020-01-15T20:04:19.192Z'),
      y: 2,
    },
    {
      x: new Date('2020-01-15T20:04:05.177Z'),
      y: 3,
    },
    {
      x: new Date('2020-01-15T20:03:51.626Z'),
      y: 4,
    },
  ];

  it('renders the scatter plot if data is present', () => {
    const wrapper = mount(<ScatterPlot points={scatterPlotPoints} />);
    expect(wrapper.find(MarkSeries)).toHaveLength(1);
  });

  it('renders lines if data is present', () => {
    const lines = [
      {
        data: [
          {
            x: new Date('2020-01-15T20:04:19.192Z'),
            y: 2,
          },
          {
            x: new Date('2020-01-15T20:04:05.177Z'),
            y: 3,
          },
          {
            x: new Date('2020-01-15T20:03:51.626Z'),
            y: 4,
          },
        ],
        name: 'a line',
      },
      {
        data: [
          {
            x: new Date('2020-01-15T20:04:19.192Z'),
            y: 5,
          },
          {
            x: new Date('2020-01-15T20:04:05.177Z'),
            y: 4,
          },
          {
            x: new Date('2020-01-15T20:03:51.626Z'),
            y: 3,
          },
        ],
        name: 'another line',
      },
    ];
    const wrapper = mount(<ScatterPlot points={scatterPlotPoints} lines={lines} />);
    expect(wrapper.find(LineSeries)).toHaveLength(2);
  });
});
