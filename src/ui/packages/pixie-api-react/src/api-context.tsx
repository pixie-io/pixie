import * as React from 'react';
import { PixieAPIClient, PixieAPIClientOptions } from '@pixie/api';

/*
 * TODO(nick): This entire package.
 *  Import `@pixie/api` and `@apollo/client/react/{stuff}`, then export the following:
 *  - A context that provides direct access to PixieAPIClient, entirely configurable.
 *  - A context provider that can be passed configuration for PixieAPIClient as props
 *  - Hooks that wrap the GQL queries via useQuery, and have parameters like the ones
 *    already present in PixieAPIClient's wrapper methods. Also useMutation, use too.
 *  - MockApolloProvider wrapper that handles Pixie's various endpoints - gRPC & GQL.
 *  - Basically, provide React-friendly bits and bobs to hide the details and Apollo.
 */

export const PixieAPIContext = React.createContext<PixieAPIClient>(null);

// eslint-disable-next-line @typescript-eslint/no-empty-interface
export interface PixieAPIContextProviderProps extends PixieAPIClientOptions {}

export const PixieAPIContextProvider: React.FC<PixieAPIContextProviderProps> = ({ children }) => (
  <>{ children }</>
);
