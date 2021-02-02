import * as React from 'react';
import { render } from 'enzyme';
// noinspection ES6PreferShortImport
import { PixieAPIContextProvider } from './api-context';

describe('Pixie API React Context', () => {
  it('renders', () => {
    expect(render(<PixieAPIContextProvider />)).toBeTruthy();
  });
});
