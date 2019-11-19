import {ContentBox} from 'components/content-box/content-box';
import {mount} from 'enzyme';
import * as React from 'react';
import {MockedProvider} from 'react-apollo/test-utils';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import {HotKeys} from 'react-hotkeys';
import {MemoryRouter} from 'react-router-dom';

import {EXECUTE_QUERY, GET_AGENT_IDS, QueryManager} from './query-manager';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

// Mock CodeMirror component because it does not mount properly in Jest.
jest.mock('react-codemirror', () => () => <div id='mock-codemirror'></div>);
jest.mock('common/vizier-gql-client', () => ({}));

describe.skip('<QueryManager/> test', () => {
  it('should update code upon dropdown selection', () => {
    const wrapper = mount(
      <MemoryRouter>
        <MockedProvider addTypename={false}>
          <QueryManager />
        </MockedProvider>
      </MemoryRouter>,
    );
    const dropdown = wrapper.find(DropdownButton).find('button').at(0);
    dropdown.simulate('click');

    expect(wrapper.find(Dropdown.Item)).toHaveLength(6);
    const dropdownItem = wrapper.find(Dropdown.Item).at(0);
    dropdownItem.simulate('click');

    expect(wrapper.find(QueryManager).at(0).state('code')).toContain('t1 = dataframe(table=\'http_events\'');
  });

  it('should pass correct headers into query editor box', async () => {
    const mocks = [
      {
        request: {
          query: GET_AGENT_IDS,
          variables: {},
        },
        result: {
          data: {
            vizier: {
              agents: [
                { info: { id: '1' } },
              ],
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MemoryRouter>
        <MockedProvider mocks={mocks} addTypename={false}>
          <QueryManager />
        </MockedProvider>
      </MemoryRouter>,
    );
    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(0).text()).toEqual('ENTER QUERY1 agent available');
  });

  it('should pass correct headers into results box when no data', async () => {
    const wrapper = mount(
      <MockedProvider addTypename={false}>
        <QueryManager />
      </MockedProvider>,
    );

    expect(wrapper.find('.content-box--header').at(1).text()).toEqual('RESULTS');
  });

  it('should pass correct headers into results box when data', async () => {
    const dataStr = '{"relation":{"columns":[{"columnName":"time_","columnType":"TIME64NS"},' +
      '{"columnName":"http_request","columnType":"STRING"}]},"rowBatches":[]}';
    const mocks = [
      {
        request: {
          query: EXECUTE_QUERY,
          variables: { queryStr: '# Enter Query Here\n' },
        },
        result: {
          data: {
            ExecuteQuery: {
              id: '1',
              table: {
                data: dataStr,
                relation: {
                  colNames: ['time_', 'http_request'],
                  colTypes: ['TIME64NS', 'STRING'],
                },
              },
              error: {
                compilerError: null,
              },
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager />
      </MockedProvider>,
    );

    const executeButton = wrapper.find('#execute-button').at(0);
    executeButton.simulate('click');
    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(1).text()).toEqual('RESULTS| Query ID: 1');
  });

  it('should handle hot keys', async () => {
    const dataStr = '{"relation":{"columns":[{"columnName":"time_","columnType":"TIME64NS"},' +
      '{"columnName":"http_request","columnType":"STRING"}]},"rowBatches":[]}';
    const mocks = [
      {
        request: {
          query: EXECUTE_QUERY,
          variables: { queryStr: '# Enter Query Here\n' },
        },
        result: {
          data: {
            ExecuteQuery: {
              id: '1',
              table: {
                data: dataStr,
                relation: {
                  colNames: ['time_', 'http_request'],
                  colTypes: ['TIME64NS', 'STRING'],
                },
              },
              error: {
                compilerError: null,
              },
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager />
      </MockedProvider>,
    );

    // Verify props to HotKey component are correct.
    const hotkeys = wrapper.find(HotKeys).at(0);
    hotkeys.props().handlers.EXECUTE_QUERY();

    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(1).text()).toEqual('RESULTS| Query ID: 1');
  });

  it('should should loading image when loading results', async () => {
    const mocks = [
      {
        request: {
          query: EXECUTE_QUERY,
          variables: { queryStr: '# Enter Query Here\n' },
        },
        result: {
          data: null,
          loading: true,
        },
      }];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager />
      </MockedProvider>,
    );

    const executeButton = wrapper.find('#execute-button').at(0);
    executeButton.simulate('click');
    wrapper.update();

    expect(wrapper.find('.spinner')).toHaveLength(1);
  });

  it('should handle error messages', async () => {
    const mocks = [
      {
        request: {
          query: EXECUTE_QUERY,
          variables: { queryStr: '# Enter Query Here\n' },
        },
        result: {
          data: {
            ExecuteQuery: {
              id: '1',
              table: null,
              error: {
                compilerError: {
                  msg: '',
                  lineColErrors: [{ line: 1, col: 1, msg: 'blah' }, { line: 2, col: 2, msg: 'blahblah' }],
                },
              },
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager />
      </MockedProvider>,
    );

    const executeButton = wrapper.find('#execute-button').at(0);
    executeButton.simulate('click');
    // TODO(michelle/philkuz)  need to figure out why our tests are flakey if wait(0)
    await wait(10);
    wrapper.update();

    expect(wrapper.find('.ReactVirtualized__Table__headerTruncatedText').at(0).text()).toEqual('Line');
    expect(wrapper.find('.ReactVirtualized__Table__headerTruncatedText').at(1).text()).toEqual('Column');
    expect(wrapper.find('.ReactVirtualized__Table__headerTruncatedText').at(2).text()).toEqual('Message');
    expect(wrapper.find('.query-results')).toHaveLength(0);
    expect(wrapper.find('.query-results--compiler-error')).toHaveLength(1);

  });

});
