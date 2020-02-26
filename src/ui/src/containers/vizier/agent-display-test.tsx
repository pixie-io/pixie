import ClientContext from 'common/vizier-grpc-client-context';
import {ContentBox} from 'components/content-box/content-box';
import {shallow} from 'enzyme';
import * as React from 'react';
import {dataFromProto} from 'utils/result-data-utils';

import {AgentDisplayContent} from './agent-display';

describe('<AgentDisplayContent />', () => {
  const agents = [
    {
      agent_id: '123',
      hostname: 'hostname',
      last_heartbeat_ns: 7893512000,
      create_time: 785612340789,
      agent_state: 'AGENT_STATE_HEALTHY',
    },
    {
      agent_id: '123',
      hostname: 'hostname',
      last_heartbeat_ns: 26495476,
      create_time: 836591640956,
      agent_state: 'AGENT_STATE_HEALTHY',
    },
  ];

  it('should pass correct headers into content box', async () => {
    const wrapper = shallow(<AgentDisplayContent agents={agents} />);

    const contentBox = wrapper.find(ContentBox);
    expect(contentBox.prop('headerText')).toBe('Available Agents');
    expect(contentBox.prop('secondaryText')).toBe('2 agents available');
  });
});
