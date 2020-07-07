import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

const NamespaceIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 32 32'>
    <path
      fill='none'
      d='M26.0002 7.04712H6.03516V24.4533H26.0002V7.04712Z'
      stroke='#B2B5BB'
      strokeWidth='1.04071'
      strokeMiterlimit='10'
      strokeLinejoin='round'
      strokeDasharray='2.08 1.04'
    />
  </SvgIcon>
);

export default NamespaceIcon;
