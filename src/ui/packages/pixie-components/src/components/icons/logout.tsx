import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const LogoutIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M17 26H7V8H17V6H7C5.9 6 5 6.9 5 8V26C5 27.11 5.9 28 7 28H17V26Z
       `}
    />
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M24.1716 15.5L19.8788 11.2072L21.293 9.79297L28 16.4999L21.293
        23.2069L19.8788 21.7927L24.1715 17.5H12.086V15.5H24.1716Z
       `}
    />
  </SvgIcon>
);
