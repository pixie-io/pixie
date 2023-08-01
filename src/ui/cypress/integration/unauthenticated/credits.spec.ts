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

describe('Credits view', () => {
  beforeEach(() => {
    cy.visit('/credits');
  });

  it('Is accessible without redirecting', () => {
    const seenRedirects = [];
    cy.on('url:changed', (url) => seenRedirects.push(url));
    cy.then(() => {
      cy.location('pathname').should('eq', '/credits');
      expect(seenRedirects.length).to.eq(0);
    });
  });

  describe('Entries', () => {
    beforeEach(() => {
      cy.get('[role=list] > [role=listitem]').as('rows');
    });

    it('Lists entries for licenses', () => {
      cy.get('[role=list]').should('exist');
      cy.get('@rows').should('have.length.gte', 1);
    });

    it('Reveals and hides licenses', () => {
      cy.get('@rows').find('[role=document]').should('not.exist');
      cy.get('@rows').find('button').contains('show').click();
      cy.get('@rows').find('[role=document]').should('exist');
      cy.get('@rows').find('button').contains('hide').click();
      cy.get('@rows').find('[role=document]').should('not.exist');
    });

    it('Links to homepages', () => {
      cy.get('@rows').find('a[href]').contains('homepage').should('exist');
    });
  });
});
