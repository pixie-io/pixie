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

import { take } from 'rxjs/operators';

import type { History, LocationState } from 'history';
import { URLParams } from './url-params';

describe('url params', () => {
  const mockWindow = {
    location: {
      search: '?script=px/script&foo=&bar=bar',
      protocol: 'https:',
      host: 'test',
      pathname: '/pathname',
    },
    history: {
      pushState: jest.fn(),
      replaceState: jest.fn(),
    },
    addEventListener: jest.fn(),
    removeEventListener: jest.fn(),
  };

  const mockHistory = {
    push: jest.fn((p: string) => mockWindow.history.pushState({ path: p }, '', p)),
    replace: jest.fn((p: string) => mockWindow.history.replaceState({ path: p }, '', p)),
  };

  const typedMockWindow = mockWindow as any as typeof window;
  const typedMockHistory = mockHistory as any as History<LocationState>;

  beforeEach(() => {
    mockWindow.history.pushState.mockClear();
    mockWindow.history.replaceState.mockClear();
    mockWindow.addEventListener.mockClear();
  });

  it('populates the scriptId and args from the url', () => {
    const instance = new URLParams(typedMockWindow, typedMockHistory);
    expect(instance.scriptId).toBe('px/script');
    expect(instance.args).toEqual({ foo: '', bar: 'bar' });
  });

  describe('setArgs', () => {
    it('updates the URL', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.setArgs({ what: 'now' });
      const expectedPath = {
        pathname: '/pathname',
        search: '?script=px%2Fscript&what=now',
      };
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('ignores the script and diff fields', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.setArgs({ script: 'another one', what: 'now', diff: 'jjj' });
      const expectedPath = {
        pathname: '/pathname',
        search: '?script=px%2Fscript&what=now',
      };
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('setScript', () => {
    it('updates the url', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.setScript('newScript', 'some changes');
      const expectedPath = {
        pathname: '/pathname',
        search: '?bar=bar&diff=some%20changes&foo=&script=newScript',
      };
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('commitAll', () => {
    it('updates the url', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.commitAll('newScript', 'some changes', { fiz: 'biz' });
      const expectedPath = {
        pathname: '/pathname',
        search: '?diff=some%20changes&fiz=biz&script=newScript',
      };
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('omits the diff field if it is empty', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.commitAll('newScript', '', { fiz: 'biz' });
      const expectedPath = {
        pathname: '/pathname',
        search: '?fiz=biz&script=newScript',
      };
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('does not update the history stack if params are unchanged', () => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.commitAll('px/script', '', { foo: '', bar: 'bar' });
      expect(mockWindow.history.pushState).not.toHaveBeenCalled();
    });
  });

  describe('onChange', () => {
    it('emits itself the first time', (done) => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.onChange
        .pipe(take(1))
        .subscribe((newParams) => {
          expect(newParams.scriptId).toBe(instance.scriptId);
          expect(newParams.scriptDiff).toBe(instance.scriptDiff);
          expect(newParams.args).toEqual(instance.args);
        }, done.fail, done);
    });

    it('emits itself when triggerOnChange is executed', (done) => {
      const instance = new URLParams(typedMockWindow, typedMockHistory);
      instance.onChange
        .pipe(take(2)) // completes after the first 2 values are emitted.
        .subscribe((newParams) => {
          expect(newParams.scriptId).toBe(instance.scriptId);
          expect(newParams.scriptDiff).toBe(instance.scriptDiff);
          expect(newParams.args).toEqual(instance.args);
        }, done.fail, done);
      instance.triggerOnChange();
    });
  });
});
