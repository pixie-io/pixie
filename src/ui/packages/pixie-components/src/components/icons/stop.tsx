import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const StopIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} width='32' height='32' viewBox='0 0 32 32'>
    <rect x='1' y='1' width='30' height='30' rx='2' stroke='currentcolor' fill='none' strokeWidth='2' />
  </SvgIcon>
);
