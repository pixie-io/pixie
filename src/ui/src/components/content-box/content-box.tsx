import * as _ from 'lodash';
import * as React from 'react';
import './content-box.scss';

export interface ContentBoxProps {
    headerText: string | JSX.Element;
    subheaderText?: string | JSX.Element;
    secondaryText?: string | JSX.Element;
    children: any;
}

export class ContentBox extends React.Component<ContentBoxProps, {}> {
  render() {
    return (
      <div className='content-box'>
        <div className='content-box--header'>
          <div className='content-box--header-text'>
            {_.toUpper(this.props.headerText)}
          </div>
          <div className='content-box--subheader-text'>
            {this.props.subheaderText ? '| ' + this.props.subheaderText : ''}
          </div>
          <div className='spacer'/>
          <div className='content-box--secondary-text'>
            {this.props.secondaryText}
          </div>
        </div>
        <div className='content-box--content'>
          {this.props.children}
        </div>
      </div>
      );
  }
}
