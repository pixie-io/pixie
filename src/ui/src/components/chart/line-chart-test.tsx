import {mount} from 'enzyme';
import * as React from 'react';
import {LineSeries} from 'react-vis';
import {DataType, Relation, RowBatchData} from 'types/generated/vizier_pb';

import {LineChart, parseData} from './line-chart';

describe('parseData', () => {
  it('returns a list of lines if the table has a time column', () => {
    const relationCols = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols[0].setColumnName('time');
    relationCols[0].setColumnType(DataType.TIME64NS);
    relationCols[1].setColumnName('float64');
    relationCols[1].setColumnType(DataType.FLOAT64);
    relationCols[2].setColumnName('int64');
    relationCols[2].setColumnType(DataType.INT64);
    const relation = new Relation();
    relation.setColumnsList(relationCols);

    const data = [
      new RowBatchData(),
    ];
    const table = {
      relation,
      data,
      name: 'test table',
      id: 'id1',
    };

    expect(parseData(table)).toHaveLength(2);
  });

  it('returns an empty array of the table does not have a time column', () => {
    const relationCols = [
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
      new Relation.ColumnInfo(),
    ];
    relationCols[1].setColumnName('float64');
    relationCols[1].setColumnType(DataType.FLOAT64);
    relationCols[2].setColumnName('int64');
    relationCols[2].setColumnType(DataType.INT64);
    const relation = new Relation();
    relation.setColumnsList(relationCols);

    const data = [
      new RowBatchData(),
    ];
    const table = {
      relation,
      data,
      name: 'test table',
      id: 'id1',
    };

    expect(parseData(table)).toHaveLength(0);
  });
});

describe('<LineChart> component', () => {
  it('renders a line chart if data is present', () => {
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
    const wrapper = mount(<LineChart lines={lines} />);
    expect(wrapper.find(LineSeries)).toHaveLength(2);
  });
});
