import { formatAPIKey } from './api-keys';

describe('formatApiKey', () => {
  it('correctly formats API keys', () => {
    const apiKeyResults = [
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        createdAtMs: new Date(new Date().getTime() - 1000 * 60 * 60 * 24 * 2), // 2 days ago
        key: 'foobar1',
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        createdAtMs: new Date(new Date().getTime() - 1000 * 60 * 60 * 3), // 3 hours ago
        key: 'foobar2',
        desc: 'abcd2',
      },
    ];
    expect(apiKeyResults.map((key) => formatAPIKey(key))).toStrictEqual([
      {
        id: '5b27f024-eccb-4d07-b28d-84ab8d88e6a3',
        idShort: '84ab8d88e6a3',
        createdAt: '2 days ago',
        key: 'foobar1',
        desc: 'abcd1',
      },
      {
        id: '1e3a32fc-caa4-5d81-e33d-10de7d77f1b2',
        idShort: '10de7d77f1b2',
        createdAt: 'about 3 hours ago',
        key: 'foobar2',
        desc: 'abcd2',
      },
    ]);
  });
});
