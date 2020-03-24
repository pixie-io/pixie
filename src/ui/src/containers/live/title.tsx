import * as React from 'react';

import {TitleContext} from './context';

const LiveViewTitle = (props) => {
  const title = React.useContext(TitleContext);
  return <div className={props.className}>script: {title}</div>;
};

export default LiveViewTitle;
