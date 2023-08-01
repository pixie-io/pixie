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

// These selectors need to be refreshed frequently (Cypress aliases reference potentially-stale elements).
const getAllRows = () => cy.get('table.MuiTable-root tbody tr');
const getNewKeyRow = (id: string) => cy.get('table.MuiTable-root tbody').find(`td[aria-label="${id}"]`).parent();
const newKeyRowShouldNotExist = (id: string) => cy.get(`td[aria-label="${id}"]`).should('not.exist');

function keyTests(prefix: string, url: string) {
  describe(`Admin: ${prefix} Keys`, () => {
    beforeEach(() => {
      cy.loginGoogle();
      // Monitor GQL calls so we can confirm which rows correspond to keys this test creates/deletes
      cy.intercept(`${Cypress.config().baseUrl}/api/graphql`, (req) => {
        if (!Object.prototype.hasOwnProperty.call(req.body, 'operationName')) return;
        switch (req.body.operationName) {
          case `get${prefix}KeysForAdminPage`: req.alias = 'gql-get-keys'; break;
          case `Create${prefix}KeyFromAdminPage`: req.alias = 'gql-create-key'; break;
          case `Delete${prefix}KeyFromAdminPage`: req.alias = 'gql-delete-key'; break;
          default: break;
        }
      });
      cy.visit(url, {
        onBeforeLoad(win: Window): void {
          cy.stub(win.navigator.clipboard, 'writeText').as('copy-to-clipboard').resolves();
        },
      });
      // Wait for table to load
      cy.wait('@gql-get-keys');
      getAllRows().its('length').should('be.at.least', 1);
    });

    let keyBetweenTests: string;
    it('Creates a key', () => {
      let numberInitialKeys: number;
      getAllRows().then((rows) => {
        numberInitialKeys = rows.length;
      });
      cy.contains('new key', { matchCase: false }).click();
      cy.wait('@gql-create-key')
        .its('response.body.data')
        .should('have.property', `Create${prefix}Key`)
        .then((data) => {
          keyBetweenTests = data.id;
          getNewKeyRow(data.id).should('exist');
        });
      getAllRows().then((rows) => {
        expect(rows.length).equal(numberInitialKeys + 1);
      });
    });

    it('Can copy a recently-created key to clipboard', () => {
      getNewKeyRow(keyBetweenTests).find('[data-testid=CopyAllOutlinedIcon]').parent().click();
      cy.get('@copy-to-clipboard').should('be.calledOnce');
    });

    it('Can delete created key after a reload', () => {
      let numberInitialKeys: number;
      getAllRows().then((rows) => {
        numberInitialKeys = rows.length;
      });
      getNewKeyRow(keyBetweenTests).find('[data-testid=DeleteForeverOutlinedIcon]').parent().click();
      cy.wait('@gql-delete-key').its('request.body.variables.id').should('eq', keyBetweenTests);
      getAllRows().then((rows) => {
        expect(rows.length).equal(numberInitialKeys - 1);
      });
      newKeyRowShouldNotExist(keyBetweenTests);
    });

    it('No longer finds the deleted key after reload', () => {
      newKeyRowShouldNotExist(keyBetweenTests);
    });

    it('Creates, copies, and deletes a key all in one page load', () => {
      // Check how many keys we started with
      let numberInitialKeys: number;
      getAllRows().then((rows) => {
        numberInitialKeys = rows.length;
      });

      // Create the key
      let singleLoadKey: string;
      cy.contains('new key', { matchCase: false }).click();
      cy.wait('@gql-create-key')
        .its('response.body.data')
        .should('have.property', `Create${prefix}Key`)
        .then((data) => {
          singleLoadKey = data.id;
          getNewKeyRow(data.id).should('exist');
        })
        .then(() => {
          // Copy to clipboard
          getNewKeyRow(singleLoadKey).find('[data-testid=CopyAllOutlinedIcon]').parent().click();
          cy.get('@copy-to-clipboard').should('be.calledOnce');
        })
        .then(() => {
          // Delete key and make sure it goes away
          getNewKeyRow(singleLoadKey).find('[data-testid=DeleteForeverOutlinedIcon]').parent().click();
          cy.wait('@gql-delete-key').its('request.body.variables.id').should('eq', singleLoadKey);
          getAllRows().then((rows) => {
            expect(rows.length).equal(numberInitialKeys);
          });
          newKeyRowShouldNotExist(singleLoadKey);
        })
        .then(() => {
          // Reload the page and make sure it's still gone (wait for table to load before checking)
          cy.reload();
          getAllRows().then((rows) => {
            expect(rows.length).equal(numberInitialKeys);
          });
          newKeyRowShouldNotExist(singleLoadKey);
        });
    });
  });
}

keyTests('API', '/admin/keys/api');
keyTests('Deployment', '/admin/keys/deployment');
