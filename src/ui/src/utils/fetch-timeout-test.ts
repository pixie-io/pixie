import { fetch } from 'whatwg-fetch';

import fetchWithTimeout from './fetch-timeout';

jest.mock('whatwg-fetch');

describe('fetchWithTimeout test', () => {
  beforeEach(() => {
    jest.useFakeTimers();
  });

  it('resolves before timeout', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 5);
    }));
    Promise.resolve().then(() => jest.advanceTimersByTime(5));
    return expect(fetchWithTimeout(10)('uri', {})).resolves.toMatch('success');
  });

  it('rejects before timeout', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((_, reject) => {
      setTimeout(() => {
        // eslint-disable-next-line prefer-promise-reject-errors
        reject('failed');
      }, 5);
    }));
    Promise.resolve().then(() => jest.advanceTimersByTime(5));
    return expect(fetchWithTimeout(10)('uri', {})).rejects.toMatch('failed');
  });

  it('time out before resolves', () => {
    expect.assertions(1);
    fetch.mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 10);
    }));
    Promise.resolve().then(() => jest.advanceTimersByTime(5));
    return expect(fetchWithTimeout(5)('uri', {})).rejects.toThrow('request timed out');
  });
});
