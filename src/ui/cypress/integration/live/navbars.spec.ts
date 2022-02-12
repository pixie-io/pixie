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

import { sidebarFooterSpec } from 'configurable/integration/live/navbars.spec';

describe('Live View navbars', () => {
  before(() => {
    cy.loginGoogle();
    cy.visit('/');
  });

  beforeEach(() => {
    cy.loginGoogle();
    cy.get('.MuiToolbar-root').as('topbar');
  });

  describe('Topbar', () => {
    beforeEach(() => {
      cy.get('.MuiToolbar-root').as('topbar');
    });

    it('Has the right contents', () => {
      cy.get('@topbar').should('exist').within(() => {
        cy.get('a[href="/"]').should('exist');
        cy.contains('Cluster:').find('+span').should(($span) => expect($span.text()).not.to.be.empty);
        // Items that have tooltips: the share/edit/move/run buttons.
        // The sidebar expander also has an aria-label but isn't a Material tooltip.
        cy.get('[aria-label]').should('have.length', 5);
        cy.get('.MuiAvatar-root').should('exist');
      });
    });

    it('Has tooltips', () => {
      cy.get('@topbar').find('[aria-label]:not([aria-label~="menu"])').as('hoverables');
      cy.get('@hoverables').should('have.length', 4);
      cy.get('@topbar').find('[aria-label="Execute script"]').should('exist'); // Wait for script to finish running.
      cy.get('@hoverables').each((el) => {
        cy.get('.MuiTooltip-popper').should('not.exist');
        cy.wrap(el).trigger('mouseover');
        cy.get('.MuiTooltip-popper').contains(el.attr('aria-label'));
        cy.wrap(el).trigger('mouseout');
        cy.get('.MuiTooltip-popper').should('not.exist');
      });
    });
  });

  describe('Sidebar', () => {
    beforeEach(() => {
      cy.get('.MuiToolbar-root [aria-label="menu"]').as('sidebar-toggle');
      cy.get('header + .MuiDrawer-root').as('sidebar').within(() => {
        cy.get('ul:nth-child(2)').as('sidebar-scripts');
        cy.get('ul:last-child').as('sidebar-footer');
      });
    });

    it('Toggles from the topbar', () => {
      cy.get('header + .MuiDrawer-root').should('exist').should((el) => expect(el.width()).lte(100));
      cy.get('@sidebar-toggle').click();
      cy.get('header + .MuiDrawer-root').should('exist').should((el) => expect(el.width()).gte(100));
      cy.get('@sidebar-toggle').click();
      cy.get('header + .MuiDrawer-root').should('exist').should((el) => expect(el.width()).lte(100));
    });

    sidebarFooterSpec();

    // TODO(nick,PC-1424): Test that the cluster and namespace buttons trigger PxL scripts
  });

  // Note: Other elements in the top/sidebar are complex enough for their own tests; not tested here.
});
