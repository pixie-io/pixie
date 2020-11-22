import * as React from 'react';

import { FrameElement } from 'utils/frame-utils';
import { AuthFooter } from './footer';

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
