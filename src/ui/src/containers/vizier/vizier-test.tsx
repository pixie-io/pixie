jest.mock('./deploy-instructions', () => {
  return { DeployInstructions: () => <></> };
});

import {mount} from 'enzyme';
import * as React from 'react';
import {MockedProvider} from 'react-apollo/test-utils';
import {Navbar} from 'react-bootstrap';
import {BrowserRouter as Router, Route} from 'react-router-dom';

import {DeployInstructions} from './deploy-instructions';
import {CHECK_VIZIER, CREATE_CLUSTER, GET_CLUSTER, Vizier, VizierMain} from './vizier';

// Mock CodeMirror component because it does not mount properly in Jest.
jest.mock('react-codemirror', () => () => <div id='mock-codemirror'></div>);

const wait = (ms: number) => new Promise((res) => setTimeout(res, ms));

describe('<VizierMain/> test', () => {
  it('should have sidebar if Vizier is connected', async () => {
    const mocks = [
      {
        request: {
          query: CHECK_VIZIER,
          variables: {},
        },
        result: {
          data: {
            vizier: {},
          },
        },
      },
    ];

    const app = mount(
      <Router>
        <MockedProvider mocks={mocks} addTypename={false}>
          <VizierMain
            pathname={'test'}
          />
        </MockedProvider>
      </Router>);

    await wait(0);
    app.update();
    expect(app.find(Navbar)).toHaveLength(1);
  });

  it('should show instructions if Vizier is not connected', async () => {
    const mocks = [
      {
        request: {
          query: CHECK_VIZIER,
          variables: {},
        },
        error: {
          name: 'error',
          message: 'cant connect to vizier',
        },
      },
    ];

    const app = mount(
      <Router>
        <MockedProvider mocks={mocks} addTypename={false}>
          <VizierMain
            pathname={'test'}
          />
        </MockedProvider>
      </Router>);

    await wait(0);
    app.update();
    expect(app.find(Navbar)).toHaveLength(0);
    expect(app.find('.cluster-instructions')).toHaveLength(1);
    expect(app.find('.cluster-instructions').text()).toEqual('Setting up DNS records for cluster...');
  });
});

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
      {
        request: {
          query: CHECK_VIZIER,
          variables: {},
        },
        result: {
          data: {
            vizier: {},
          },
        },
      },
    ];

    const app = mount(
      <Router>
        <MockedProvider mocks={mocks} addTypename={false}>
          <Vizier
            location={{ pathname: 'query' }}
          />
        </MockedProvider>
      </Router>);

    await wait(0);
    app.update();
    expect(app.find(VizierMain)).toHaveLength(1);
  });

  it('should show deploy instructions if vizier not connected', async () => {
    // Mock out createObjectURL function used in deploy instructions.
    window.URL.createObjectURL = jest.fn();

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
            location={{ pathname: 'query' }}
          />
        </MockedProvider>
      </Router>);

    await wait(0);
    app.update();
    expect(app.find(DeployInstructions)).toHaveLength(1);
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
            location={{ pathname: 'query' }}
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
            location={{ pathname: 'query' }}
          />
        </MockedProvider>
      </Router>);

    await wait(0);
    app.update();

    expect(app.find('.cluster-instructions')).toHaveLength(1);
    expect(app.find('.cluster-instructions').text()).toEqual('Initializing...');
  });
});
