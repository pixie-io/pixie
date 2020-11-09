import * as React from 'react';

import { Footer } from 'components/auth/footer';
import { FrameElement } from './frame-utils';

export default {
  title: 'Auth/Footer',
  component: Footer,
  decorators: [(Story) => <FrameElement width={500}><Story /></FrameElement>],
};

export const Basic = () => <Footer />;
