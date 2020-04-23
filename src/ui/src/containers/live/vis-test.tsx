import {getQueryFuncs, TABLE_DISPLAY_TYPE} from './vis';

const testVisNoVars = {
  variables: [],
  widgets: [{
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        value: 'abc',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  },
  {
    func: {
      name: 'get_error_rate',
      args: [{
        name: 'bar',
        value: 'def',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

const testVisWithVars = {
  variables: [{
    name: 'myvar1',
    type: 'PX_STRING',
    defaultValue: 'abc',
  }, {
    name: 'myvar2',
    type: 'PX_STRING',
    defaultValue: 'def',
  }],
  widgets: [    {
    name: 'latency',
    func: {
      name: 'get_latency',
      args: [{
        name: 'foo',
        variable: 'myvar1',
      }, {
        name: 'bar',
        variable: 'myvar2',
        default: 'def',
      }],
    },
    displaySpec: {
      '@type': TABLE_DISPLAY_TYPE,
    },
  }],
};

describe('getQueryFuncs', () => {
  it('should fill in values from constants', () => {
    expect(getQueryFuncs(testVisNoVars, {})).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'widget_0',
        args: [{name: 'foo', value: 'abc'}],
      },
      {
        name: 'get_error_rate',
        outputTablePrefix: 'widget_1',
        args: [{name: 'bar', value: 'def'}],
      },
    ]);
  });

  it('should fill in values from variables with defaults', () => {
    expect(getQueryFuncs(testVisWithVars, {})).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'latency',
        args: [
          {name: 'foo', value: 'abc'},
          {name: 'bar', value: 'def'},
        ],
      },
    ]);
  });

  it('should fill in values from variable values overriding defaults', () => {
    expect(getQueryFuncs(testVisWithVars, {
      myvar2: 'xyz',
    })).toStrictEqual([
      {
        name: 'get_latency',
        outputTablePrefix: 'latency',
        args: [
          {name: 'foo', value: 'abc'},
          {name: 'bar', value: 'xyz'},
        ],
      },
    ]);
  });
});
