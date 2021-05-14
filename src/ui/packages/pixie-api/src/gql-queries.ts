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

import gql from 'graphql-tag';

// noinspection ES6PreferShortImport
import { DEFAULT_USER_SETTINGS } from './user-settings';

export const USER_QUERIES = {
  GET_USER_INFO: gql`
    {
      user {
        id
        email
        name
        picture
        orgName
        orgID
      }
    }
  `,
  GET_ALL_USER_SETTINGS: gql`
    {
      userSettings(keys: [${Object.keys(DEFAULT_USER_SETTINGS).map((k) => JSON.stringify(k)).join(', ')}]) {
        key
        value
      }
    }
  `,
  SAVE_USER_SETTING: gql`
    mutation UpdateUserSetting($key: String!, $value: String!) {
      UpdateUserSettings(keys: [$key], values: [$value])
    }
  `,
  INVITE_USER: gql`
    mutation InviteUser($email: String!, $firstName: String!, $lastName: String!) {
      InviteUser(email: $email, firstName: $firstName, lastName: $lastName) {
        email
        inviteLink
      }
    }
  `,
  GET_ORG_USERS: gql`
    {
      orgUsers {
        id
        name
        email
        isApproved
      }
    }
  `,
  SET_USER_INFO: gql`
    mutation UpdateUser ($id: ID!, $isApproved: Boolean) {
      UpdateUser(userInfo: {id: $id, isApproved: $isApproved})
    }
  `,
};

export const ORG_QUERIES = {
  GET_ORG_INFO: gql`
    {
      org {
        id
        name
        enableApprovals
      }
    }
  `,
  SET_ORG_INFO: gql`
    mutation UpdateOrg ($id: ID!, $enableApprovals: Boolean) {
      UpdateOrg(orgInfo: {id: $id, enableApprovals: $enableApprovals})
    }
  `,
};

export const API_KEY_QUERIES = {
  LIST_API_KEYS: gql`
    {
      apiKeys {
        id
        key
        desc
        createdAtMs
      }
    }
  `,

  DELETE_API_KEY: gql`
    mutation deleteKey($id: ID!) {
      DeleteAPIKey(id: $id)
    }
  `,

  CREATE_API_KEY: gql`
    mutation {
      CreateAPIKey {
        id
      }
    }
  `,
};

export const DEPLOYMENT_KEY_QUERIES = {
  LIST_DEPLOYMENT_KEYS: gql`
    {
      deploymentKeys {
        id
        key
        desc
        createdAtMs
      }
    }
  `,

  DELETE_DEPLOYMENT_KEY: gql`
    mutation deleteKey($id: ID!) {
      DeleteDeploymentKey(id: $id)
    }
  `,

  CREATE_DEPLOYMENT_KEY: gql`
    mutation {
      CreateDeploymentKey {
        id
      }
    }
  `,
};

export const CLUSTER_QUERIES = {
  GET_CLUSTER_CONN: gql`
    query GetClusterConnection($id: ID!) {
      clusterConnection(id: $id) {
        ipAddress
        token
      }
    }
  `,
  LIST_CLUSTERS: gql`
    {
      clusters {
        id
        clusterUID
        clusterName
        clusterVersion
        prettyClusterName
        status
        lastHeartbeatMs
        numNodes
        numInstrumentedNodes
        vizierVersion
        vizierConfig {
          passthroughEnabled
        }

      }
    }
  `,
  // TODO(nserrino,PC-471): Update to filtered lookup on clusterName once that graphql endpoint has landed.
  GET_CLUSTER_CONTROL_PLANE_PODS: gql`
    {
      clusters {
        clusterName
        controlPlanePodStatuses {
          name
          status
          message
          reason
          containers {
            name
            state
            reason
            message
          }
          events {
            message
          }
        }
      }
    }
  `,
};

export const AUTOCOMPLETE_QUERIES = {
  AUTOCOMPLETE: gql`
    query autocomplete($input: String, $cursor: Int, $action: AutocompleteActionType, $clusterUID: String) {
      autocomplete(input: $input, cursorPos: $cursor, action: $action, clusterUID: $clusterUID) {
        formattedInput
        isExecutable
        tabSuggestions {
          tabIndex
          executableAfterSelect
          suggestions {
            kind
            name
            description
            matchedIndexes
            state
          }
        }
      }
    }
  `,
  FIELD: gql`
    query getCompletions($input: String, $kind: AutocompleteEntityKind, $clusterUID: String) {
      autocompleteField(input: $input, fieldType: $kind, clusterUID: $clusterUID) {
        name
        description
        matchedIndexes
        state
      }
    }
  `,
};
