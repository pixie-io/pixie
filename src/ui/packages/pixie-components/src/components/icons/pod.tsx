import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const PodIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      d={
        `M5.00023 8.21597L16.0902 5L27.1802 8.21597L16.0902 11.4319L5.00023
        8.21597ZM5 9.44775V21.2482L15.3328 26.9718L15.3839 12.5361L5
        9.44775ZM27.1808 21.2482V9.44775L16.7969 12.5361L16.848 26.9718L27.1808 21.2482Z`
      }
    />
  </SvgIcon>
);
