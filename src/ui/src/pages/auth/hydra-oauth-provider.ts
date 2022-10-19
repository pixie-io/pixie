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

import type * as React from 'react';

import {
  UiNode, UiNodeAttributes, UiNodeGroupEnum, UiNodeInputAttributes,
  UiNodeTypeEnum, V0alpha2ApiFactory,
} from '@ory/kratos-client';
import { UserManager } from 'oidc-client';
import * as QueryString from 'query-string';

import { FormField, FormStructure } from 'app/components';
import { HydraInvitationForm } from 'app/containers/admin/hydra-invitation-form';
import { HydraButtons, RejectHydraSignup } from 'app/containers/auth/hydra-buttons';
import { AUTH_CLIENT_ID, AUTH_URI } from 'app/containers/constants';

import { CallbackArgs, getSignupArgs, getLoginArgs } from './callback-url';

export const PasswordError = new Error('Kratos identity server error: Password method not found in flows.');
export const FlowIDError = new Error('Auth server requires a flow parameter in the query string, but none were found.');

const kratosClient = V0alpha2ApiFactory(null, '/oauth/kratos');

// Renders a form with an error and no fields.
const displayErrorFormStructure = (error: Error): FormStructure => ({
  action: '/',
  method: 'POST',
  submitBtnText: 'Back To Login',
  fields: [],
  errors: [{ text: error.message }],
  defaultSubmit: false,
  onClick: () => {
    window.location.href = '/auth/login';
  },
});

function isInputAttributes(attr: UiNodeAttributes): attr is UiNodeInputAttributes {
  return attr.node_type === UiNodeTypeEnum.Input;
}

function nodeToFormField(node: UiNode): FormField | null {
  if (!isInputAttributes(node.attributes)) {
    return null;
  }
  // We render our own form button.
  if (node.attributes.type === 'submit') {
    return null;
  }
  return {
    disabled: node.attributes.disabled,
    messages: node.messages,
    name: node.attributes.name,
    pattern: node.attributes.pattern,
    required: node.attributes.required,
    type: node.attributes.type,
    value: node.attributes.value,
  };
}

function nodesToFormFields(nodes: Array<UiNode>): Array<FormField> {
  return nodes
    .filter(node => node.type === UiNodeTypeEnum.Input)
    .filter(node => node.group === UiNodeGroupEnum.Password || node.group === UiNodeGroupEnum.Default)
    .map(nodeToFormField)
    .filter(node => node);
}

export const HydraClient = {
  userManager: new UserManager({
    authority: AUTH_URI,
    client_id: AUTH_CLIENT_ID,
    redirect_uri: `${window.location.origin}/auth/callback`,
    loadUserInfo: false,
    scope: 'vizier',
    response_type: 'token',
  }),

  loginRequest(): void {
    this.userManager.signinRedirect({
      prompt: 'login',
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  signupRequest(): void {
    this.userManager.signinRedirect({
      prompt: 'login',
      state: {
        redirectArgs: getSignupArgs(),
      },
    });
  },

  refetchToken(): void {
    this.userManager.signinRedirect({
      state: {
        redirectArgs: getLoginArgs(),
      },
    });
  },

  handleToken(): Promise<CallbackArgs> {
    return new Promise<CallbackArgs>((resolve, reject) => {
      this.userManager.signinRedirectCallback()
        .then((user) => {
          if (!user) {
            reject(new Error('user is undefined, please try logging in again'));
          }
          resolve({
            redirectArgs: user.state.redirectArgs,
            token: {
              accessToken: user.access_token,
            },
          });
        }).catch(reject);
    });
  },

  // Get the PasswordLoginFlow from Kratos.
  async getPasswordLoginFlow(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const flow = parsed.flow as string;
    if (flow == null) {
      return displayErrorFormStructure(FlowIDError);
    }
    const { data } = await kratosClient.getSelfServiceLoginFlow(flow);

    return {
      action: data.ui.action,
      method: data.ui.method,
      errors: data.ui.messages,
      fields: nodesToFormFields(data.ui.nodes),
      submitBtnText: 'Login',
      // Kratos and browser redirects limits us to submit the login form
      // through an XmlHttpRequest, the default HTML Form submit behavior.
      defaultSubmit: true,
    };
  },

  async getResetPasswordFlow(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const flow = parsed.flow as string;
    if (flow == null) {
      return displayErrorFormStructure(FlowIDError);
    }
    const { data } = await kratosClient.getSelfServiceSettingsFlow(flow);

    return {
      action: data.ui.action,
      method: data.ui.method,
      errors: data.ui.messages,
      fields: nodesToFormFields(data.ui.nodes),
      submitBtnText: 'Change Password',
      // Kratos and browser redirects limits us to submit the login form
      // through an XmlHttpRequest, the default HTML Form submit behavior.
      defaultSubmit: true,
    };
  },

  async getError(): Promise<FormStructure> {
    const parsed = QueryString.parse(window.location.search);
    const error = parsed.error as string;
    if (error == null) {
      return displayErrorFormStructure(new Error('server error'));
    }
    const { data } = await kratosClient.getSelfServiceError(error);

    return displayErrorFormStructure(new Error(JSON.stringify(data)));
  },

  isInvitationEnabled(): boolean {
    return true;
  },

  getInvitationComponent(): React.FC {
    return HydraInvitationForm;
  },

  getLoginButtons(): React.ReactElement {
    return HydraButtons({
      onUsernamePasswordButtonClick: () => this.loginRequest(),
      usernamePasswordText: 'Login',
    });
  },

  getSignupButtons(): React.ReactElement {
    return RejectHydraSignup({});
  },
};
