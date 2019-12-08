import './version-info.scss';

import * as React from 'react';
import {PIXIE_CLOUD_VERSION} from 'utils/env';

export const VersionInfo = () => (
  <div className='pixie-version-info'>
    {PIXIE_CLOUD_VERSION}
  </div>
);
