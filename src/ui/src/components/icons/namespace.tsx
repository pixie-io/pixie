import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

const NamespaceIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 24 24'>
    <path
      fill='none'
      d='M21.45 2H2V22H21.45V2Z'
      stroke='#B2B5BB'
      strokeWidth='1.04071'
      strokeMiterlimit='10'
      strokeLinejoin='round'
      strokeDasharray='2.08 1.04'
    />
  </SvgIcon>
);

export default NamespaceIcon;
