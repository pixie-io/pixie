import * as React from 'react';
import { ApolloProvider } from '@apollo/client/react';
import { PixieAPIClientAbstract, PixieAPIClient, PixieAPIClientOptions } from '@pixie-labs/api';

export const PixieAPIContext = React.createContext<PixieAPIClientAbstract>(null);

export type PixieAPIContextProviderProps = PixieAPIClientOptions;

export const PixieAPIContextProvider: React.FC<PixieAPIContextProviderProps> = ({ children, ...opts }) => {
  const [pixieClient, setPixieClient] = React.useState<PixieAPIClient>(null);

  React.useEffect(() => {
    PixieAPIClient.create(opts).then(setPixieClient);
    return () => {
      // TODO(nick): Unlucky timing could have this happen and THEN the promise above resolve. Need to cancel it.
      setPixieClient(null);
    };
    // Destructuring the options instead of checking directly because the identity of the object changes every render.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [opts.uri, opts.onUnauthorized]);

  return !pixieClient ? null : (
    <PixieAPIContext.Provider value={pixieClient}>
      <ApolloProvider client={pixieClient.getCloudGQLClientForAdapterLibrary().graphQL}>
        { children }
      </ApolloProvider>
    </PixieAPIContext.Provider>
  );
};
