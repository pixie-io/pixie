import { Resolvers } from 'apollo-client';
import gql from 'graphql-tag';
import * as uuid from 'uuid';

export const localGQLTypeDef = gql`
  # Apollo defines this already, but it needs to be declared here for tooling to pick it up
  directive @client on FIELD

  extend type Mutation {
    updateDrawer(drawerOpened: Boolean!): Boolean
    appendHistory(history: ScriptExecutionIn!): ID
  }

  extend type Query {
    drawerOpened: Boolean
    scriptHistory: [ScriptExecution!]
  }

  type ScriptExecution {
    id: ID!
    time: Date!
    code: String
    title: String
    status: ExecutionStatus!
  }

  # A single GraphQL field cannot be both an input and an output type. Even if identical, there must be two types.
  input ScriptExecutionIn {
      id: ID!
      time: Date!
      code: String
      title: String
      status: ExecutionStatus!
  }

  enum ExecutionStatus {
    FAILED
    SUCCESS
  }

  scalar Date
`;

export const QUERY_DRAWER_OPENED = gql`{ drawerOpened @client }`;
export const MUTATE_DRAWER_OPENED = gql`
  mutation UpdateDrawer($drawerOpened: Boolean!) {
    updateDrawer(drawerOpened: $drawerOpened) @client
  }
`;

function updateDrawer(_, { drawerOpened }, { cache }) {
  cache.writeQuery({
    query: QUERY_DRAWER_OPENED,
    data: { drawerOpened },
  });
  return drawerOpened;
}

export const APPEND_HISTORY = gql`
  mutation AppendHistory($history: ScriptExecutionIn!) {
    appendHistory(history: $history) @client
  }
`;

export const SCRIPT_HISTORY = gql`
  {
    scriptHistory @client {
      id
      time
      code
      title
      status
    }
  }
`;

function appendHistory(_, args, { cache }) {
  const newHistory = {
    ...args.history,
    id: uuid(),
    __typename: 'ScriptExecution',
  };
  let history = [];
  try {
    history = cache.readQuery({ query: SCRIPT_HISTORY }).scriptHistory;
  } catch (e) {
    //
  }
  cache.writeQuery({
    query: SCRIPT_HISTORY,
    data: {
      // Add the newest history to the top.
      scriptHistory: [newHistory, ...history],
    },
  });
  return newHistory.id;
}

export const localGQLResolvers: Resolvers = {
  Query: {
    // These resolvers are only called on cache misses.
    drawerOpened: () => true,
    scriptHistory: () => [],
  },
  Mutation: {
    updateDrawer,
    appendHistory,
  },
};
