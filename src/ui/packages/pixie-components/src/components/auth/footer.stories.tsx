import * as React from 'react';

import { AuthFooter } from './footer';
import { FrameElement } from 'utils/frame-utils';

export default {
  title: 'Auth/Footer',
  component: AuthFooter,
  decorators: [
    (Story) => (
      <FrameElement width={500}>
        <Story />
      </FrameElement>
    ),
  ],
};

export const Basic = () => <AuthFooter />;
