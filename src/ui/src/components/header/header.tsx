import * as React from 'react';
import './header.scss';

export interface HeaderProps {
  primaryHeading: string;
  secondaryHeading: string;
}

export class Header extends React.Component<HeaderProps, {}> {
  render() {
    // TODO(michelle): This is basically a breadcrumb. When we have more complicated breadcrumb
    // functionality, we should change this into a breadcrumbs component.
    const primaryHeading = this.props.primaryHeading;
    const secondaryHeading = this.props.secondaryHeading;
    return (
      <div className='header'>
      <div className='header--text'>
        <div className='header--primary-text'>{primaryHeading}</div>
        |
        <div className='header--secondary-text'>{secondaryHeading}</div>
        </div>
      </div>
      );
  }
 }
