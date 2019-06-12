import {mount} from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';
import * as CodeMirror from 'react-codemirror';
import {AgentDisplay, GET_AGENTS} from './agent-display';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

describe('<AgentDisplay/> test', () => {
  it('should pass correct headers into content box', async () => {
    const mocks = [
      {
        request: {
          query: GET_AGENTS,
          variables: {},
        },
        result: {
          data: {
            vizier: {
              agents: [
                { state: 'HEALTHY', info: { id: '1', hostInfo: { hostname: 'test'} }, lastHeartbeatMs: 1, uptimeS: 1 },
              ],
            },
          },
        },
      },
    ];

    const wrapper = mount(
      <MockedProvider mocks={mocks} addTypename={false}>
        <AgentDisplay
          onAgents={'test'}
        />
      </MockedProvider>,
    );
    await wait(0);
    wrapper.update();

    expect(wrapper.find('.content-box--header').at(0).text()).toEqual('AVAILABLE AGENTS1 agent available');
  });
});
