import {mount} from 'enzyme';
import * as React from 'react';
import {LineSeries, MarkSeries} from 'react-vis';

import {GQLDataTable} from '../../../../vizier/services/api/controller/schema/schema';
import {parseData, ScatterPlot} from './scatter';

describe('parseData', () => {
  it('returns scatter plot data if the tables conforms to it', () => {
    const tables = [{
      relation: {
        colNames: [
          'time_',
          'http_resp_latency_ms',
        ],
        colTypes: [
          'TIME64NS',
          'FLOAT64',
        ],
      },
      data: `{
        "relation":{
          "columns":[
            {
              "columnName":"time_",
              "columnType":"TIME64NS"
            },
            {
              "columnName":"http_resp_latency_ms",
              "columnType":"FLOAT64"
            }
          ]
        },
        "rowBatches":[
          {
            "cols":[
              {
                "time64nsData":{
                  "data":[
                    "1579117035911671407",
                    "1579117039197491529"
                  ]
                }
              },
              {
                "float64Data":{
                  "data":[
                    3.216794,
                    600024.201942
                  ]
                }
              }
            ],
            "numRows":"2",
            "eow":true,
            "eos":true
          }
        ],
        "name":"output"
      }`,
      name: 'output',
    }] as GQLDataTable[];

    expect(parseData(tables)).not.toBeNull();
  });

  it('returns null if no scatter plot data', () => {
    const tables = [{
      relation: {
        colNames: [
          'table_name',
          'column_name',
          'column_type',
          'column_desc',
        ],
        colTypes: [
          'STRING',
          'STRING',
          'STRING',
          'STRING',
        ],
      },
      data: `{
        "relation":{
          "columns":[
            {
              "columnName":"table_name",
              "columnType":"STRING"
            },
            {
              "columnName":"column_name",
              "columnType":"STRING"
            },
            {
              "columnName":"column_type",
              "columnType":"STRING"
            },
            {
              "columnName":"column_desc",
              "columnType":"STRING"
            }
          ]
        },
        "rowBatches":[
          {
            "cols":[
              {
                "stringData":{
                  "data":[
                    "process_stats",
                    "process_stats",
                    "process_stats",
                    "process_stats",
                    "process_stats"
                  ]
                }
              },
              {
                "stringData":{
                  "data":[
                    "time_",
                    "upid",
                    "major_faults",
                    "minor_faults",
                    "cpu_utime_ns"
                  ]
                }
              },
              {
                "stringData":{
                  "data":[
                    "TIME64NS",
                    "UINT128",
                    "INT64",
                    "INT64",
                    "INT64"
                  ]
                }
              },
              {
                "stringData":{
                  "data":[
                    "Timestamp when the data record was collected.",
                    "An opaque numeric ID that globally identify a running process inside the cluster.",
                    "Number of major page faults",
                    "Number of minor page faults",
                    "Time spent on user space by the process"
                  ]
                }
              }
            ],
            "numRows":"5",
            "eow":true,
            "eos":true
          }
        ],
        "name":"output"
      }`,
      name: 'output',
    }] as GQLDataTable[];

    expect(parseData(tables)).toBeNull();
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
