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

import { stubExecuteScript } from 'support/utils/grpc';

// Tests that authentication is enforced for routes that should do so.
// For example, most pages should redirect to `/login` if the user isn't authorized.
// However, some routes are accessible regardless, such as the auth routes themselves.
describe('Authentication guards', () => {
  beforeEach(() => {
    // TODO(nick): For later tests, this can be expensive (like refreshing on an expensive script).
    //  Can `cy.go` (with cy.visit in the before rather than beforeEach) work?
    cy.visit('/');
  });

  it('Loads at all', () => {
    cy.get('div#root').should('exist');
  });

  it('Redirects to login', () => {
    cy.location('pathname').should('eq', '/auth/login');
  });

  it('Can access email/password auth', () => {
    const loginButtons = cy.get('.MuiBox-root > button');
    loginButtons.should('have.length', 2);
    // TODO: We can't click the buttons (redirect leaves the domain under test, which Cypress doesn't work well with).
    //  We don't want to do that anyway; what we really want to do is verify our own callback flows with cy.request().
    //  See src/ui/cypress/commands/login.command.js for more info.
  });

  describe('Logged in with a Google account', () => {
    // Intercepts have to be set up before a test runs.
    // Otherwise, the intercept isn't wired until all commands have queued.
    before(() => {
      cy.loginGoogle();
      stubExecuteScript();
    });

    it('Sees us as authenticated', () => {
      cy.request({
        url: '/api/authorized',
        failOnStatusCode: false,
      }).its('status').should('eq', 200);
    });
  });
});
