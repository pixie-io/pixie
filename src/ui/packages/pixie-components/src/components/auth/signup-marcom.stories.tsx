import * as React from 'react';

import { FrameElement } from 'utils/frame-utils';
import { SignupMarcom } from './signup-marcom';

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
