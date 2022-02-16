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

import {
  ExecuteScriptRequestJson,
  ExecuteScriptRequestString,
  ExecuteScriptResponseJson,
  ExecuteScriptResponseString,
} from 'fixtures/script-execution/cluster.fixture';
import {
  interceptExecuteScript,
  atou,
  waitExecuteScript,
  deserializeExecuteScriptRequest,
  deserializeExecuteScriptResponse,
} from 'support/utils/grpc';

describe('Script execution helpers', () => {
  it('Can parse base64 correctly', () => {
    const arr = atou('AAAAR/AK');
    expect([].slice.call(arr)).deep.equal([0, 0, 0, 0x47, 0xF0, 0x0A]);
  });

  it('Deserializes a request correctly', () => {
    const json = deserializeExecuteScriptRequest(ExecuteScriptRequestString).toObject();
    expect(json).deep.equal(ExecuteScriptRequestJson);
  });

  it('Deserializes a response correctly', () => {
    const messages = deserializeExecuteScriptResponse(ExecuteScriptResponseString).map((m) => m.toObject());
    expect(messages.length).equal(ExecuteScriptResponseJson.length);
    for (let i = 0; i < ExecuteScriptResponseJson.length; i++) {
      expect(messages[i]).deep.equal(ExecuteScriptResponseJson[i]);
    }
  });
});

describe('Sidebar script shortcuts', () => {
  before(() => {
    cy.loginGoogle();
    interceptExecuteScript().as('exec-auto');
    cy.visit('/');
  });

  beforeEach(() => {
    // Once in before all for the auto exec;
    // Once each for the manual clicks that fire more requests.
    // Remember, Cypress intercepts only trigger once each by default.
    cy.loginGoogle();
  });

  it('Auto-runs cluster script before anything is pressed', () => {
    waitExecuteScript('@exec-auto').then(({ response, reqJson }) => {
      expect(response.statusCode).equal(200);
      expect(reqJson.queryStr).contains("''' Cluster Overview");
    });
  });

  it('Executes namespace script when clicked', () => {
    interceptExecuteScript().as('exec-namespace');
    cy.get('header + .MuiDrawer-root a[aria-label="Namespaces"]').click();
    waitExecuteScript('@exec-namespace').then(({ response, reqJson }) => {
      expect(response.statusCode).equal(200);
      expect(reqJson.queryStr).contains("''' Namespaces Overview");
    });
  });

  it('Executes cluster script again when clicked', () => {
    interceptExecuteScript().as('exec-cluster');
    cy.get('header + .MuiDrawer-root a[aria-label="Cluster"]').click();
    waitExecuteScript('@exec-cluster').then(({ response, reqJson }) => {
      expect(response.statusCode).equal(200);
      expect(reqJson.queryStr).contains("''' Cluster Overview");
    });
  });
});
