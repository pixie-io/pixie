import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {mount} from 'enzyme';
import * as React from 'react';
import { MockedProvider } from 'react-apollo/test-utils';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {DeployInstructions} from './deploy-instructions';
import {CREATE_CLUSTER, GET_CLUSTER, Vizier} from './vizier';

// Mock CodeMirror component because it does not mount properly in Jest.
jest.mock('react-codemirror', () => () => <div id='mock-codemirror'></div>);

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
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();
    expect(app.find(SidebarNav)).toHaveLength(1);
  });

  it('should show deploy instructions if vizier not connected', async () => {
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
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();
    expect(app.find(DeployInstructions)).toHaveLength(1);
    expect(app.find(DeployInstructions).get(0).props.clusterID).toBe('test');
  });

  it('should show pending message if cluster unhealthy', async () => {
    const mocks = [
      {
        request: {
          query: GET_CLUSTER,
          variables: {},
        },
        result: {
          data: {
            cluster: {
                status: 'VZ_ST_UNHEALTHY',
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
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();

    expect(app.find('.cluster-instructions')).toHaveLength(1);
    expect(app.find('.cluster-instructions').text()).toEqual(
        'Cluster found. Waiting for pods and services to become ready...');
  });

  it('should try to create cluster if does not exist', async () => {
    const mocks = [
      {
        request: {
          query: GET_CLUSTER,
          variables: {},
        },
        error: {
            name: 'error',
            message: 'org has no clusters',
        },
      },
      {
        request: {
          query: CREATE_CLUSTER,
          variables: {},
        },
        result: {},
      },
    ];

    const app = mount(
        <Router>
            <MockedProvider mocks={mocks} addTypename={false}>
              <Vizier
                location={ { pathname: 'query' } }
              />
            </MockedProvider>
        </Router>);

    await wait(0);
    app.update();

    expect(app.find('.cluster-instructions')).toHaveLength(1);
    expect(app.find('.cluster-instructions').text()).toEqual('Initializing...');
  });
});
