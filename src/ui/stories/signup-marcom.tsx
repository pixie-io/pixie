import * as React from 'react';

import { SignupMarcom } from 'components/auth/signup-marcom';
import { FrameElement } from './frame-utils';

export default {
  title: 'Auth/Signup Marcom',
  component: SignupMarcom,
  decorators: [(Story) => <FrameElement width={500}><Story /></FrameElement>],
};

export const Basic = () => <SignupMarcom />;
