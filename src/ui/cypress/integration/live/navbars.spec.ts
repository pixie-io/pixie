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

import { sidebarFooterSpec } from 'configurable/integration/live/navbars.spec';

describe('Live View navbars', () => {
  before(() => {
    cy.loginGoogle();
    stubExecuteScript();
  });

  beforeEach(() => {
    // Re-apply one-time intercepts each run.
    cy.loginGoogle();
    stubExecuteScript();
    cy.visit('/');
    cy.get('.MuiToolbar-root').as('topbar');
  });

  describe('Topbar', () => {
    beforeEach(() => {
      cy.get('.MuiToolbar-root').as('topbar');
    });

    it('Has the right contents', () => {
      cy.get('@topbar').should('exist').within(() => {
        cy.get('a[href="/"]').should('exist');
        cy.contains('cluster:').find('+span').should(($span) => expect($span.text()).not.to.be.empty);
        // Items that have tooltips: the share/edit/move/run buttons.
        // The sidebar expander also has an aria-label but isn't a Material tooltip.
        // The trigger for the Command Palette sets aria-label in the same way.
        cy.get('[aria-label]').should('have.length', 6);
        cy.get('.MuiAvatar-root').should('exist');
      });
    });

    it('Has tooltips', () => {
      cy.get('@topbar').find('[aria-label]:not([aria-label~="menu"]):not([aria-label~="command"])').as('hoverables');
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
      cy.get('header + .MuiDrawer-root > .MuiPaper-root').as('sidebar').within(() => {
        cy.get('> a').as('sidebar-scripts');
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

    interface ConfigFlags {
      DOMAIN_NAME: string;
      ANNOUNCEMENT_ENABLED: boolean;
      CONTACT_ENABLED: boolean;
    }
    it('Has a filled out footer', () => {
      cy.window().its('__PIXIE_FLAGS__').then((CONFIG?: ConfigFlags) => {
        cy.get('@sidebar-footer').within(() => {
          cy.get('[aria-label="Announcements"]')
            .should(CONFIG.ANNOUNCEMENT_ENABLED ? 'exist' : 'not.exist');
          cy.get('[aria-label="Docs"]')
            .should('exist')
            .should('have.attr', 'href', `https://docs.${CONFIG.DOMAIN_NAME}`);
          cy.get('[aria-label="Help"]')
            .should(CONFIG.CONTACT_ENABLED ? 'exist' : 'not.exist');
          if (CONFIG.CONTACT_ENABLED) {
            cy.get('[aria-label="Help"]')
              .should('have.attr', 'id', 'intercom-trigger');
          }
        });
      });
    });

    // Any configurable extra items that show up in the footer have their own tests
    sidebarFooterSpec();
  });

  // Note: Other elements in the top/sidebar are complex enough for their own tests; not tested here.
});
