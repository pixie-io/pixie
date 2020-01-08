import gql from 'graphql-tag';

export const localGQLResolvers = {};

export const localGQLTypeDef = gql`
  scalar Date

  type EditorTab {
    id: String!
    name: String
    code: String
    baseScriptId: String
    saved: Boolean
  }

  type Script {
    id: String!
    name: String
    code: String
  }

  type ExecutionHistory {
    scriptId: String!
    code: String
    executionTime: Date!
  }
`;
