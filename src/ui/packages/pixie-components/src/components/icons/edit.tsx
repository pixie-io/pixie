import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const EditIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 21 21'>
    <path d='M20.408 1.70718L18.9938 0.292969L7.29297 11.9938L8.70718 13.408L20.408 1.70718Z' />
    <path
      d={`M2 5.00012V19.0001H16V12.0001H18V19.0001C18 20.1001 17.1 21.0001 16 21.0001H2C0.89 21.0001 0 20.1001 0
    19.0001V5.00012C0 3.90012 0.89 3.00012 2 3.00012H9V5.00012H2Z`}
    />
  </SvgIcon>
);
