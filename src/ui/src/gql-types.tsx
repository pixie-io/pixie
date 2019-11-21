import gql from 'graphql-tag';

import {GQLQueryResult} from '../../vizier/services/api/controller/schema/schema';

export const EXECUTE_QUERY = gql`
mutation ExecuteQuery($queryStr: String!) {
    ExecuteQuery(queryStr: $queryStr) {
      id
      table {
        relation {
          colNames
          colTypes
        }
        data
      }
      error {
        compilerError {
          msg
          lineColErrors {
            line
            col
            msg
          }
        }
      }
    }
}
`;

export interface ExecuteQueryResult {
  ExecuteQuery: GQLQueryResult;
}
