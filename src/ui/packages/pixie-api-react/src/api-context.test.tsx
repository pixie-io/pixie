import * as React from 'react';
import { screen, render } from '@testing-library/react';
// noinspection ES6PreferShortImport
import { PixieAPIContextProvider } from './api-context';

describe('Pixie API React Context', () => {
  it('renders once the context is ready', async () => {
    render(<PixieAPIContextProvider>Hello</PixieAPIContextProvider>);
    await screen.findByText('Hello');
  });
});
