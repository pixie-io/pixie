import * as React from 'react';
import {Route} from 'react-router-dom';
import {shallowAsync} from 'utils/testing';

import {SubdomainApp} from './subdomain-app';

// Mock DataVoyager component because it does not mount properly in Jest.
// (See: https://github.com/vega/voyager/issues/812)
jest.mock('datavoyager', () => ({ CreateVoyager: () => ({updateData: () => { return; } }) }));

jest.mock('common/cloud-gql-client', () => ({
  getCloudGQLClient: jest.fn().mockResolvedValue({}),
}));

describe('<SubdomainApp/> test', () => {
  it('should have correct routes', async () => {
    const app = await shallowAsync(<SubdomainApp />);

    expect(app.find(Route)).toHaveLength(4);
  });
});
