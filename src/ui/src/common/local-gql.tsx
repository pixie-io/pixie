import {Resolvers} from 'apollo-client';
import gql from 'graphql-tag';
import * as uuid from 'uuid';

export const localGQLTypeDef = gql`
  extend type Mutation {
    updateDrawer(drawerOpened: Boolean!): Boolean
    appendHistory(history: ScriptExecution!): ID
  }

  extend type Query {
    drawerOpened: Boolean
    scriptHistory: [ScriptExecution!]
  }

  type ScriptExecution {
    id: ID!
    time: Date!
    code: string
    title: string
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
  mutation AppendHistory($history: ScriptExecution!) {
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
