import { ApolloQueryResult } from '@apollo/client/core';
import { CloudClient } from './cloud-gql-client';
import { mockApolloClient, Invocation } from './testing';

describe('Cloud client (GQL wrapper)', () => {
  const { query, mutate } = mockApolloClient();

  it('instantiates', () => {
    const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
    expect(cloudClient).toBeTruthy();
  });

  describe('GQL methods', () => {
    const querySubjects: ReadonlyArray<Invocation<CloudClient>> = [
      ['getClusterConnection', 'foo'],
      ['listAPIKeys'],
      ['listDeploymentKeys'],
      ['listClusters'],
      ['getClusterControlPlanePods'],
    ];

    it.each(querySubjects)('%s queries GraphQL', async (name: keyof CloudClient, ...args: any[]) => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      expect(typeof cloudClient[name]).toBe('function');
      query.mockImplementation(() => Promise.resolve({
        data: new Proxy({}, {
          get: () => 'retrieved',
        }),
      } as ApolloQueryResult<unknown>));

      const out = await (cloudClient[name] as (...p: any[]) => any)(...args);
      expect(out).toBe('retrieved');
    });

    it('getSetting gets and parses settings', async () => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      query.mockImplementation(() => Promise.resolve({
        data: {
          userSettings: { tourSeen: 'true' },
        },
      } as ApolloQueryResult<{ userSettings: any }>));

      const out = await cloudClient.getSetting('tourSeen');
      expect(out).toEqual(true);
    });

    it('getSetting returns default values in the absence of a setting', async () => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      query.mockImplementation(() => Promise.resolve({
        data: { userSettings: {} },
      } as ApolloQueryResult<{ userSettings: any }>));

      const out = await cloudClient.getSetting('tourSeen');
      expect(out).toEqual(false);
    });

    it('setSetting sends a mutation to GraphQL', async () => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      mutate.mockImplementation(() => Promise.resolve({}));

      await cloudClient.setSetting('tourSeen', true);
      expect(mutate).toHaveBeenCalledWith(jasmine.objectContaining({
        variables: {
          key: 'tourSeen',
          value: 'true',
        },
      }));
    });

    it.each(['createAPIKey', 'createDeploymentKey'])('%s sends a mutation to GraphQL', async (name) => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      mutate.mockImplementation(() => Promise.resolve({
        data: {
          [`C${name.substr(1)}`]: { // CreateAPIKey and CreateDeploymentKey; names of the mutations are title case
            id: 'foo',
          },
        },
      }));

      const out = await cloudClient[name]();
      expect(mutate).toHaveBeenCalled();
      expect(out).toBe('foo');
    });

    it.each(['deleteAPIKey', 'deleteDeploymentKey'])('%s sends a mutation to GraphQL', async (name) => {
      const cloudClient = new CloudClient({ apiKey: '', uri: 'irrelevant' });
      mutate.mockImplementation(() => Promise.resolve({}));

      await cloudClient[name]('foo');
      expect(mutate).toHaveBeenCalledWith(jasmine.objectContaining({
        variables: {
          id: 'foo',
        },
      }));
    });
  });
});
