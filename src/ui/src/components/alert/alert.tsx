import * as React from 'react';
import './alert.scss';

export interface AlertProps {
  children: any;
}

// TODO(michelle): Update the alert component to have more alert types.
export class Alert extends React.Component<AlertProps, {}> {
  render() {
    return (
      <div className='pl-alert'>
        <div className='pl-alert--content'>
          {this.props.children}
        </div>
      </div>
    );
  }
}
