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
import { AUTH_CLIENT_ID, AUTH_URI, OAUTH_PROVIDER } from 'app/containers/constants';

import { CallbackArgs, getSignupArgs, getLoginArgs } from './callback-url';

export const PasswordError = new Error('Kratos identity server error: Password method not found in flows.');
export const FlowIDError = new Error('Auth server requires a flow parameter in the query string, but none were found.');

const kratosUri = `${location.protocol}//${location.host}/oauth/kratos`;
const kratosClient = V0alpha2ApiFactory(null, kratosUri);

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

// Dynamic client registration types
interface OAuth2Client {
  client_id: string;
  client_secret?: string;
  client_name?: string;
  redirect_uris?: string[];
  grant_types?: string[];
  response_types?: string[];
  scope?: string;
  token_endpoint_auth_method?: string;
}

interface DynamicClientRegistrationResponse extends OAuth2Client {
  client_id_issued_at?: number;
  client_secret_expires_at?: number;
  registration_client_uri?: string;
  registration_access_token?: string;
}

// Cache for the dynamically registered client ID
let cachedClientId: string | null = null;

async function getDynamicClientId(): Promise<string> {
  // Only use dynamic registration for hydra provider
  if (OAUTH_PROVIDER !== 'hydra') {
    return AUTH_CLIENT_ID;
  }

  // Return cached client ID if available
  if (cachedClientId) {
    return cachedClientId;
  }

  // Use /oauth/hydra prefix which will be stripped by the proxy
  // Preserve the current port (e.g., :8080 for dev server)
  const registrationEndpoint = `${window.location.protocol}//${window.location.host}/oauth/hydra/oauth2/register`;
  console.log(`Dynamic client registration endpoint: ${registrationEndpoint}`);
  
  const redirect_uri = `${window.location.protocol}//${window.location.host}/auth/callback`;
  console.log(`Using redirect URI: ${redirect_uri}`);

  const clientMetadata: Partial<OAuth2Client> = {
    client_name: 'pixie-auth-client',
    redirect_uris: [
      redirect_uri,
    ],
    grant_types: ['authorization_code', 'refresh_token', 'implicit'],
    response_types: ['code', 'id_token', 'token'],
    scope: 'openid offline notifications gist vizier',
    token_endpoint_auth_method: 'none',
  };

  try {
    const response = await fetch(registrationEndpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
      body: JSON.stringify(clientMetadata),
    });

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`Dynamic client registration failed: ${response.status} ${response.statusText} - ${errorText}`);
    }

    const data: DynamicClientRegistrationResponse = await response.json();
    
    if (!data.client_id) {
      throw new Error('No client_id returned from dynamic registration');
    }

    // Cache the client ID for future use
    cachedClientId = data.client_id;
    
    console.log('Successfully registered dynamic OAuth2 client:', data.client_id);
    return data.client_id;
  } catch (error) {
    console.error('Failed to register dynamic client:', error);
    // Fall back to static client ID if dynamic registration fails
    console.warn('Falling back to static client ID');
    return AUTH_CLIENT_ID;
  }
}

export const HydraClient = {
  userManager: null as UserManager | null,
  userManagerPromise: null as Promise<UserManager> | null,

  async getUserManager(): Promise<UserManager> {
    // Return existing manager if available
    if (this.userManager) {
      return this.userManager;
    }

    // Return existing promise if initialization is in progress
    if (this.userManagerPromise) {
      return this.userManagerPromise;
    }

    // Start initialization
    this.userManagerPromise = (async () => {
      const clientId = await getDynamicClientId();
      
      this.userManager = new UserManager({
        authority: AUTH_URI,
        client_id: clientId,
        redirect_uri: `${window.location.origin}/auth/callback`,
        loadUserInfo: false,
        scope: 'vizier',
        response_type: 'token',
      });

      return this.userManager;
    })();

    return this.userManagerPromise;
  },

  loginRequest(): void {
    // Fire and forget - the redirect will happen once the client ID is obtained
    this.getUserManager().then(userManager => {
      userManager.signinRedirect({
        prompt: 'login',
        state: {
          redirectArgs: getLoginArgs(),
        },
      });
    }).catch(error => {
      console.error('Failed to initiate login:', error);
      // Could potentially show an error to the user here
    });
  },

  signupRequest(): void {
    // Fire and forget - the redirect will happen once the client ID is obtained
    this.getUserManager().then(userManager => {
      userManager.signinRedirect({
        prompt: 'login',
        state: {
          redirectArgs: getSignupArgs(),
        },
      });
    }).catch(error => {
      console.error('Failed to initiate signup:', error);
      // Could potentially show an error to the user here
    });
  },

  refetchToken(): void {
    // Fire and forget - the redirect will happen once the client ID is obtained
    this.getUserManager().then(userManager => {
      userManager.signinRedirect({
        state: {
          redirectArgs: getLoginArgs(),
        },
      });
    }).catch(error => {
      console.error('Failed to refetch token:', error);
      // Could potentially show an error to the user here
    });
  },

  handleToken(): Promise<CallbackArgs> {
    return new Promise<CallbackArgs>(async (resolve, reject) => {
      try {
        const userManager = await this.getUserManager();
        const user = await userManager.signinRedirectCallback();
        
        if (!user) {
          reject(new Error('user is undefined, please try logging in again'));
          return;
        }
        
        resolve({
          redirectArgs: user.state.redirectArgs,
          token: {
            accessToken: user.access_token,
          },
        });
      } catch (error) {
        reject(error);
      }
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
