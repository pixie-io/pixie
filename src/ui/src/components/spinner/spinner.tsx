import './spinner.scss';

// @ts-ignore : TS does not like image files.
import * as darkSvg from 'images/icons/loading-dark.svg';
// @ts-ignore : TS does not like image files.
import * as lightSvg from 'images/icons/Loading.svg';
import * as React from 'react';

interface SpinnerProps {
  variant?: 'light' | 'dark';
}

export const Spinner: React.FC<SpinnerProps> = ({ variant = 'light' }) => {
  const imgUrl = variant === 'light' ? lightSvg : darkSvg;
  return <img className='pixie-spinner' src={imgUrl} />;
};
