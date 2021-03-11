import * as React from 'react';
import { MockPixieAPIClient } from '@pixie/api';
import { PixieAPIContext, PixieAPIContextProviderProps } from 'api-context';
import { MockedProvider, MockedResponse } from '@apollo/client/testing';

export interface MockPixieAPIContextProviderProps extends Partial<PixieAPIContextProviderProps> {
  mocks?: MockedResponse[];
}

export const MockPixieAPIContextProvider: React.FC<MockPixieAPIContextProviderProps> = ({ children, mocks = [] }) => {
  const [mocked, setMocked] = React.useState<MockPixieAPIClient>(null);
  React.useEffect(() => {
    MockPixieAPIClient.create().then(setMocked);
  }, []);
  return mocked && (
    <MockedProvider addTypename={false} mocks={mocks}>
      <PixieAPIContext.Provider value={mocked}>
        {children}
      </PixieAPIContext.Provider>
    </MockedProvider>
  );
};
