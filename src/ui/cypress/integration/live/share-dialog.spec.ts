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

describe('Share dialog', () => {
  beforeEach(() => {
    cy.loginGoogle();
    stubExecuteScript(200).as('exec-auto');
  });

  it('Opens a share dialog and can copy from it', () => {
    cy.visit('/', {
      onBeforeLoad(win: Window): void {
        cy.stub(win.navigator.clipboard, 'writeText').as('copy-to-clipboard').resolves();
      },
    });

    cy.get('.MuiToolbar-root [aria-label="Share this script"] > button').click();
    cy.get('.MuiDialog-root > .MuiDialog-container').as('dialog');
    cy.get('@dialog').should('exist');
    cy.get('@dialog').find('button').as('copy-button');
    cy.get('@copy-button').should('exist').should('have.text', 'Copy Link');

    cy.get('@copy-button').click();
    cy.get('.MuiSnackbar-root').should('exist').then((el) => {
      expect(el.text()).to.be.oneOf([
        // In Electron, clipboard permission is always granted.
        // In Chrome, clipboard permission requires the user to approve.
        // In Firefox, clipboard permission is denied for automated clicks no matter what.
        'Copied!',
        'Error: could not copy link automatically.',
      ]);
    });

    // TODO(NickLanam): This only tests for a Google org; need to test email/password too.
    cy.location('href').then((href) => {
      cy.get('@copy-to-clipboard').should('be.calledOnceWithExactly', href);
    });
  });
});
