import {ContentBox} from 'components/content-box/content-box';
import {AutoSizedScrollableTable} from 'components/table/scrollable-table';
import {shallow} from 'enzyme';
import * as React from 'react';

import {AgentDisplayContent} from './agent-display';

const NOW = 1582930654484;

jest.spyOn(global.Date, 'now').mockReturnValue(NOW);

describe('<AgentDisplayContent />', () => {
  const agents = [
    {
      agent_id: '123',
      hostname: 'hostname1',
      last_heartbeat_ns: 7893512000,
      create_time: NOW + 3600000,
      agent_state: 'AGENT_STATE_HEALTHY',
    },
    {
      agent_id: '234',
      hostname: 'hostname2',
      last_heartbeat_ns: 26495476,
      create_time: NOW + 72000000,
      agent_state: 'AGENT_STATE_HEALTHY',
    },
  ];

  it('should pass correct headers into content box', () => {
    const wrapper = shallow(<AgentDisplayContent agents={agents} />);

    const contentBox = wrapper.find(ContentBox);
    expect(contentBox.prop('headerText')).toBe('Available Agents');
    expect(contentBox.prop('secondaryText')).toBe('2 agents available');

    expect(wrapper.find(AutoSizedScrollableTable).prop('data')).toHaveLength(2);
    expect(wrapper.find(AutoSizedScrollableTable).prop('data')[0]).toEqual({
      id: '123',
      hostname: 'hostname1',
      heartbeat: '7.89',
      uptime: 'about 1 hour',
      state: 'AGENT_STATE_HEALTHY',
    });
  });
});
