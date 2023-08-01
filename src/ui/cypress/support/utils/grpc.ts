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

import type { Interception, RouteHandler } from 'cypress/types/net-stubbing';

import { deserializeExecuteScriptRequest, deserializeExecuteScriptResponse } from 'app/testing/utils/grpc';
import { ExecuteScriptRequest, ExecuteScriptResponse } from 'app/types/generated/vizierapi_pb';

const SERVICE_ROOT = 'px.api.vizierpb.VizierService';

export interface InterceptExecuteScriptOptions {
  /** If provided, becomes the second argument to cy.intercept. Use for stubbing. */
  routeHandler?: RouteHandler;
}
export function interceptExecuteScript(opts: InterceptExecuteScriptOptions = {}): Cypress.Chainable<null> {
  const url = `${Cypress.config().baseUrl}/${SERVICE_ROOT}/ExecuteScript`;

  // TODO: Use opts to filter what gets intercepted and what doesn't.
  return cy.intercept(url, opts.routeHandler ?? undefined);
}

/** Skip the network call for ExecuteScript. Default response code is HTTP 429 Too Many Requests. */
export function stubExecuteScript(responseCode = 429): Cypress.Chainable<null> {
  return interceptExecuteScript({ routeHandler: (req) => req.reply(responseCode) });
}

export interface DeserializedIntercept extends Interception {
  reqJson: ExecuteScriptRequest.AsObject;
  resJson: ExecuteScriptResponse.AsObject[];
}

export function waitExecuteScript(alias: string): Cypress.Chainable<DeserializedIntercept> {
  return cy.wait(alias).then(({ request, response, ...rest }: Interception) => {
    const reqJson = deserializeExecuteScriptRequest(request.body).toObject();
    const resJson = response?.body.length > 0
      ? deserializeExecuteScriptResponse(response.body).map((m) => m.toObject())
      : [];
    return {
      ...rest,
      request,
      response,
      reqJson,
      resJson,
    };
  });
}
