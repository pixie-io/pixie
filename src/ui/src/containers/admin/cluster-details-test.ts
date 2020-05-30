import {formatAgent} from './cluster-details';

describe('formatAgent', () => {
  it('correctly formats agent info', () => {
    const agentResults = [
      {
        agent_id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        asid: 1780,
        hostname: 'gke-host',
        ip_address: '',
        agent_state: 'AGENT_STATE_HEALTHY',
        create_time: new Date(new Date().getTime() - 1000*60*60*24*2), // 2 days ago
        last_heartbeat_ns: 100074517116,
      },
      {
        agent_id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        asid: 1780,
        hostname: 'gke-host2',
        ip_address: '',
        agent_state: 'AGENT_STATE_UNKNOWN',
        create_time: new Date(new Date().getTime() - 1000*60*60*3), // 3 hours ago
        last_heartbeat_ns: 1574517116,
      },
    ];
    expect(agentResults.map(agent => formatAgent(agent))).toStrictEqual([
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        status: 'HEALTHY',
        statusGroup: 'healthy',
        hostname: 'gke-host',
        lastHeartbeat: '1 min 40 sec ago',
        uptime: '2 days',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        status: 'UNKNOWN',
        statusGroup: 'unknown',
        hostname: 'gke-host2',
        lastHeartbeat: '1 sec ago',
        uptime: 'about 3 hours',
      }
    ]);
  });
});
