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

declare global {
  // eslint-disable-next-line @typescript-eslint/no-namespace
  namespace Cypress {
    interface Chainable {
      loginGoogle(): Chainable<Element>
    }
  }
}

// Note: this works, but relies on a real user's auth session cookies.
// For CI, we'll need test users and to be able to log in as them:
//   https://docs.cypress.io/guides/testing-strategies/google-authentication#Setting-Google-app-credentials-in-Cypress
//   https://docs.cypress.io/guides/testing-strategies/auth0-authentication
Cypress.Commands.add('loginGoogle', () => {
  const GOOGLE_COOKIE_KEY = Cypress.env('GOOGLE_SESSION_COOKIE_KEY');

  cy.setCookie(GOOGLE_COOKIE_KEY, Cypress.env('GOOGLE_SESSION_COOKIE'));

  // For Google auth, this is the only cookie needed to validate a session.
  cy.getCookie(GOOGLE_COOKIE_KEY).should('have.property', 'value');

  // Every request from here on out needs CSRF headers.
  cy.intercept(`${Cypress.config().baseUrl}/api/**`, { middleware: true }, (req) => {
    req.headers['x-csrf'] = 'undefined';
    req.headers.Referer = req.headers.origin;
  });
});

// Workaround for the override below:
// Parameters<T> ignores all signatures except the last of overloaded methods.
// See https://github.com/microsoft/TypeScript/issues/29732 for discussion.
type RequestArgs =
  [options: Partial<Cypress.RequestOptions>]
  | [url: string, body?: Cypress.RequestBody]
  | [method: Cypress.HttpMethod, url: string, body?: Cypress.RequestBody];


// cy.intercept doesn't touch cy.request.
// We always need the CSRF headers even when unauthenticated,
// so we override the request method to always inject them.
// We would do cy.visit as well, but that doesn't need the same headers.
Cypress.Commands.overwrite('request', (originalFn, ...args: RequestArgs) => {
  const defaults = {
    headers: {
      'x-csrf': 'undefined',
      'Referer': Cypress.config().baseUrl,
    },
  };

  // cy.request has several signatures; have to handle them all.
  let options: Partial<Cypress.RequestOptions> = {};
  if (args[0] != null && typeof args[0] === 'object') {
    options = args[0];
  } else if (args.length === 1) {
    [options.url] = args as [string];
  } else if (args.length === 2) {
    // (method, url) signature has overlap with (url, body).
    // Disambiguation is a pain.
    [options.method, options.url] = args as [string, string];
  } else if (args.length === 3) {
    [options.method, options.url, options.body] = args as [Cypress.HttpMethod, string, Cypress.RequestBody];
  }

  return originalFn({
    ...defaults,
    ...options,
    ...{
      headers: {
        ...defaults.headers,
        ...options.headers,
      },
    },
  });
});

// Required for the namespace augmentation to be valid (TS2669)
// This module doesn't actually export anything;
// Importing it augments Cypress as a side effect.
export default {};
