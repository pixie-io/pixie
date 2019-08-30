import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {mount} from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {DeployInstructions} from './deploy-instructions';
import {GET_CLUSTER, Vizier} from './vizier';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

describe('<Vizier/> test', () => {
  it('should have sidebar if Vizier is healthy', async () => {
    const mocks = [
      {
        request: {
          query: GET_CLUSTER,
          variables: {},
        },
        result: {
          data: {
            cluster: {
              status: 'VZ_ST_HEALTHY',
              lastHeartbeatMs: 1,
              id: 'test',
            },
          },
        },
      },
    ];

    const app = mount(
        <Router>
            <MockedProvider mocks={mocks} addTypename={false}>
              <Vizier
                match=''
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();
    expect(app.find(SidebarNav)).toHaveLength(1);
  });

  it('should show deploy instructions if vizier not healthy', async () => {
    const mocks = [
      {
        request: {
          query: GET_CLUSTER,
          variables: {},
        },
        result: {
          data: {
            cluster: {
                status: 'VZ_ST_DISCONNECTED',
                lastHeartbeatMs: -1,
                id: 'test',
            },
          },
        },
      },
    ];

    const app = mount(
        <Router>
            <MockedProvider mocks={mocks} addTypename={false}>
              <Vizier
                match=''
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();
    expect(app.find(DeployInstructions)).toHaveLength(1);
  });
});
