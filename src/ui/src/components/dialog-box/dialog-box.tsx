import './dialog-box.scss';

import * as infoImage from 'images/new-logo.svg';
import * as React from 'react';

export interface DialogBoxProps {
  width?: number;
  children: any;
}

export class DialogBox extends React.Component<DialogBoxProps, {}> {
  render() {
    const style = this.props.width ? { width: this.props.width } : null;
    return (
      <div className='dialog-box' style={style}>
        <div className='dialog-box--header'>
          <img src={infoImage} style={{ width: '55px' }} />
        </div>
        <div className='dialog-box--content'>
          {this.props.children}
        </div>
      </div>
    );
  }
}
