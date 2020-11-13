import * as React from 'react';

import { AuthFooter } from 'pixie-components';
import { FrameElement } from './frame-utils';

export default {
  title: 'Auth/Footer',
  component: AuthFooter,
  decorators: [(Story) => <FrameElement width={500}><Story /></FrameElement>],
};

export const Basic = () => <AuthFooter />;
