import Vizier from 'containers/vizier';
import * as React from 'react';
import {shallowAsync} from 'utils/testing';

import {SubdomainApp} from './subdomain-app';

// Mock DataVoyager component because it does not mount properly in Jest.
// (See: https://github.com/vega/voyager/issues/812)
jest.mock('datavoyager', () => ({ CreateVoyager: () => ({ updateData: () => { return; } }) }));

jest.mock('common/cloud-gql-client', () => ({
  getCloudGQLClient: jest.fn().mockResolvedValue({}),
  getClusterConnection: jest.fn().mockResolvedValue({}),
}));

describe('<SubdomainApp/> test', () => {
  it('renders', async () => {
    const app = await shallowAsync(<SubdomainApp />);

    expect(app.find(Vizier)).toHaveLength(1);
  });
});
