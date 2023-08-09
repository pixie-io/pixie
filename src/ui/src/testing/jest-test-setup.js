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

import { TextDecoder, TextEncoder } from 'util';

import { MockOAuthClient } from 'app/pages/auth/mock-oauth-provider';

import 'regenerator-runtime/runtime';

global.TextDecoder = TextDecoder;
global.TextEncoder = TextEncoder;

// Setup mock object for local storage.
const localStorageMock = (() => {
  let store = {};
  return {
    getItem: (key) => store[key],
    setItem: (key, value) => {
      store[key] = value.toString();
    },
    clear: () => {
      store = {};
    },
    removeItem: (key) => {
      delete store[key];
    },
  };
})();

const analyticsMock = (() => ({
  page: () => { },
}))();

const mockPixieFlags = {
  AUTH0_DOMAIN: '', AUTH0_CLIENT_ID: '', DOMAIN_NAME: '', SEGMENT_UI_WRITE_KEY: '', LD_CLIENT_ID: '',
};

Object.defineProperty(window, 'localStorage', { value: localStorageMock });
Object.defineProperty(window, 'analytics', { value: analyticsMock });
Object.defineProperty(window, '__PIXIE_FLAGS__', { value: mockPixieFlags });

// This prevents console errors about the use of useLayoutEffect on the server
jest.mock('react', () => ({
  ...jest.requireActual('react'),
  useLayoutEffect: jest.requireActual('react').useEffect,
}));

// The sidebar checks this, and many page tests include the sidebar. So, we mock auth.
jest.mock('app/pages/auth/utils', () => ({
  GetCSRFCookie: () => '',
  GetOAuthProvider: () => MockOAuthClient,
}));
