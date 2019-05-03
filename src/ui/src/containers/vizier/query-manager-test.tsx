import {ContentBox} from 'components/content-box/content-box';
import {mount} from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';
import {Button, Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import {EXECUTE_QUERY, GET_AGENT_IDS, QueryManager} from './query-manager';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

// Mock CodeMirror component because it does not mount properly in Jest.
jest.mock('react-codemirror', () => () => <div id='mock-codemirror'></div>);

describe('<QueryManager/> test', () => {
  it('should update code upon dropdown selection', () => {
    const wrapper = mount(
      <MockedProvider addTypename={false}>
        <QueryManager/>
      </MockedProvider>,
    );
    const dropdown = wrapper.find(DropdownButton).find('button').at(0);
    dropdown.simulate('click');

    expect(wrapper.find(Dropdown.Item)).toHaveLength(5);
    const dropdownItem = wrapper.find(Dropdown.Item).at(1);
    dropdownItem.simulate('click');

    expect(wrapper.find(QueryManager).at(0).state('code')).toContain('t1 = From(table=\'bcc_http_trace\',');
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
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager/>
      </MockedProvider>,
    );
    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(0).text()).toEqual('ENTER QUERY1 agent available');
  });

  it('should pass correct headers into results box when no data', async () => {
    const wrapper = mount(
      <MockedProvider addTypename={false}>
        <QueryManager/>
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
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <QueryManager/>
      </MockedProvider>,
    );

    const executeButton = wrapper.find('#execute-button').at(0);
    executeButton.simulate('click');
    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(1).text()).toEqual('RESULTS| Query ID: 1');
  });
});
