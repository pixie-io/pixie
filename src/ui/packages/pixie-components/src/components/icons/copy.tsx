import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const CopyIcon = (props: SvgIconProps) => (
  <SvgIcon {...props}  width='32' height='32' viewBox='0 0 32 32' fill='none' xmlns='http://www.w3.org/2000/svg'>
    <path
      fillRule='evenodd'
      clipRule='evenodd'
      d={`M20.336 5H8.048C6.91648 5 6 5.91648 6 7.048V21.384H8.048V7.048H20.336V5ZM23.408 9.096H12.144C11.0125
        9.096 10.096 10.0125 10.096 11.144V25.48C10.096 26.6115 11.0125 27.528 12.144 27.528H23.408C24.5395
        27.528 25.456 26.6115 25.456 25.48V11.144C25.456 10.0125 24.5395 9.096 23.408 9.096ZM12.144
        25.48H23.408V11.144H12.144V25.48Z`}
      fill='#4A4C4F'
    />
  </SvgIcon>
);
