import * as React from 'react';

import { AuthBox } from './auth-box';
import { FrameElement } from 'utils/frame-utils';

export default {
  title: 'Auth/AuthBox',
  component: AuthBox,
  decorators: [
    (Story) => (
      <FrameElement width={500}>
        <Story />
      </FrameElement>
    ),
  ],
};

export const Login = () => <AuthBox variant='login' />;
export const Signup = () => <AuthBox variant='signup' />;
