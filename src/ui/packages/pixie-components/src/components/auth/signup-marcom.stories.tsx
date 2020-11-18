import * as React from 'react';

import { SignupMarcom } from './signup-marcom';
import { FrameElement } from 'utils/frame-utils';

export default {
  title: 'Auth/Signup Marcom',
  component: SignupMarcom,
  decorators: [
    (Story) => (
      <FrameElement width={500}>
        <Story />
      </FrameElement>
    ),
  ],
};

export const Basic = () => <SignupMarcom />;
