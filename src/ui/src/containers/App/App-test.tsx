import * as React from 'react';
import {Route} from 'react-router-dom';
import {shallowAsync} from 'utils/testing';

import {App} from './App';

jest.mock('common/cloud-gql-client', () => ({
  getCloudGQLClient: jest.fn().mockResolvedValue({}),
}));

describe('<App/> test', () => {
  it('should have correct routes', async () => {
    const app = await shallowAsync(<App />);

    expect(app.find(Route)).toHaveLength(3);
  });
});
