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

describe('Live view keyboard shortcuts', () => {
  const os = Cypress.platform; // One of aix, darwin, freebsd, linux, openbsd, sunos, win32
  const useCmdKey = os === 'darwin';

  const modalTitle = 'Available Shortcuts';

  // Note: this is a before, not a beforeEach.
  // Each test needs to close whatever its shortcut opened to reset properly and test that.
  before(() => {
    cy.loginGoogle();
    cy.visit('/');
    // Wait for live view to load
    cy.url().should('contain', '/live/clusters/');
  });

  beforeEach(() => cy.loginGoogle());

  it('Opens shortcut help from profile menu', () => {
    cy.get('header.MuiPaper-root > .MuiToolbar-root > *:last-child').as('profile-menu-trigger');
    cy.get('@profile-menu-trigger').click();
    cy.contains('Keyboard Shortcuts').click();
    cy.contains(modalTitle).should('exist');
    cy.get('body').type('{esc}');
    cy.contains('Keyboard Shortcuts').should('not.exist');
    cy.contains(modalTitle).should('not.exist');
  });

  it('Opens shortcut help with a shortcut of its own', () => {
    cy.get('body').type('{shift}?');
    cy.contains(modalTitle).should('exist');
    cy.get('body').type('{esc}');
    cy.contains(modalTitle).should('not.exist');
  });

  it('Documents every shortcut', () => {
    cy.get('body').type('{shift}?');
    // Note: the way :nth-child works is weird; n+2 means "everything after the first child"
    cy.contains(modalTitle).parent().find('> div:nth-child(n+2)').as('rows');
    cy.get('@rows').should('have.length', 4);
    cy.get('@rows').within(() => {
      cy.contains('Show/hide script editor').should('exist');
      cy.contains('Show/hide data drawer').should('exist');
      cy.contains('Execute current Live View script').should('exist');
      cy.contains('Show all keyboard shortcuts').should('exist');
    });
    cy.get('body').type('{esc}');
    cy.contains(modalTitle).should('not.exist');
  });

  it('Shows and hides the data drawer', () => {
    const hotkey = `${useCmdKey ? '{cmd}' : '{ctrl}'}d`;
    const selector = '.MuiDrawer-paperAnchorDockedBottom';
    cy.get(selector).should('not.be.visible');
    cy.get('body').type(hotkey);
    cy.get(selector).should('be.visible');
    cy.get('body').type(hotkey);
    cy.get(selector).should('not.be.visible');
  });

  it('Shows and hides the editor', () => {
    const hotkey = `${useCmdKey ? '{cmd}' : '{ctrl}'}e`;
    const selector = '.MuiDrawer-paperAnchorDockedRight';
    cy.get(selector).should('not.be.visible');
    cy.get('body').type(hotkey);
    cy.get(selector).should('be.visible');
    cy.get('body').type(hotkey);
    cy.get(selector).should('not.be.visible');
  });

  // TODO(nick,PC-1424): Check that Ctrl/Cmd+Enter executes the same script script that's already chosen
  it('Re-runs the current script');
});
