/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import fetch from 'cross-fetch';

import fetchWithTimeout from './fetch-timeout';

jest.mock('cross-fetch', () => jest.fn());

describe('fetchWithTimeout test', () => {
  beforeEach(() => {
    jest.useFakeTimers();
  });

  it('resolves before timeout', () => {
    expect.assertions(1);
    (fetch as jest.Mock).mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 5);
    }));
    Promise.resolve().then(() => jest.advanceTimersByTime(5));
    return expect(fetchWithTimeout(10)('uri', {})).resolves.toMatch('success');
  });

  it('rejects before timeout', () => {
    expect.assertions(1);
    (fetch as jest.Mock).mockImplementationOnce(() => new Promise((_, reject) => {
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
    (fetch as jest.Mock).mockImplementationOnce(() => new Promise((resolve) => {
      setTimeout(() => {
        resolve('success');
      }, 10);
    }));
    Promise.resolve().then(() => jest.advanceTimersByTime(5));
    return expect(fetchWithTimeout(5)('uri', {})).rejects.toThrow('request timed out');
  });
});
