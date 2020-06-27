import * as React from 'react';
import './header.scss';

export interface HeaderProps {
  primaryHeading: string;
  secondaryHeading: string;
}

export const Header = ({ primaryHeading, secondaryHeading }: HeaderProps) => (
  <div className='header'>
    <div className='header--text'>
      <div className='header--primary-text'>{primaryHeading}</div>
      |
      <div className='header--secondary-text'>{secondaryHeading}</div>
    </div>
  </div>
);
