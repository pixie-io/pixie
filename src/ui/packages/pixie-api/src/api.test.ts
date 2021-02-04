import { PixieAPIClient } from 'api';

describe('Pixie TypeScript API Client', () => {
  it('can be instantiated', async () => {
    const client = await PixieAPIClient.create('', []);
    expect(client).toBeTruthy();
  });
});
