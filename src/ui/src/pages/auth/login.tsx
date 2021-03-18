import * as React from 'react';
import { AuthBox } from '@pixie-labs/components';
import { BasePage } from './base';
import { OAuthLoginRequest } from './utils';

export const LoginPage = () => (
  <BasePage>
    <AuthBox
      variant='login'
      toggleURL={`/auth/signup${window.location.search}`}
      onPrimaryButtonClick={OAuthLoginRequest}
    />
  </BasePage>
);
