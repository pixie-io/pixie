import * as React from 'react';

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

const PixieCommand = (props: SvgIconProps) => (
  <SvgIcon {...props} viewBox='0 0 32 26'>
    <path d='M5.3999 7.47998H12.3599V14.44H10.3699V9.24998H5.3999V7.47998Z' />
    <path d='M7.3899 11.56H5.3999V18.52H7.3899V11.56Z' />
    <path d='M25.1974 7.32078L15.2344 17.2838L16.6415 18.6909L26.6045 8.7279L25.1974 7.32078Z' />
    <path d='M16.6381 7.31144L15.231 8.71857L18.1018 11.5894L19.5089 10.1823L16.6381 7.31144Z' />
    <path d='M23.7294 14.4015L22.3223 15.8086L25.1931 18.6794L26.6002 17.2723L23.7294 14.4015Z' />
  </SvgIcon>
);

export default PixieCommand;
