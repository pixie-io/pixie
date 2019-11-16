import {fetch} from 'whatwg-fetch';

import fetchWithTimeout from './fetch-timeout';

jest.mock('whatwg-fetch');

describe('fetchWithTimeout test', () => {
  it('resolves before timeout', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 5);
    }));

    return expect(fetchWithTimeout(10)('uri', {})).resolves.toMatch('success');
  });

  it('rejects before timeout', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((_, reject) => {
      setTimeout(() => {
        reject('failed');
      }, 5);
    }));

    return expect(fetchWithTimeout(10)('uri', {})).rejects.toMatch('failed');
  });

  it('time out before resolves', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 10);
    }));

    return expect(fetchWithTimeout(5)('uri', {})).rejects.toThrow('request timed out');
  });
});
