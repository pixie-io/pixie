import * as React from 'react';
import './alert.scss';

export interface AlertProps {
  children: any;
}

// TODO(michelle): Update the alert component to have more alert types.
export const Alert = ({ children }: AlertProps) => (
  <div className='pl-alert'>
    <div className='pl-alert--content'>
      {children}
    </div>
  </div>
);
