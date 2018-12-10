import * as React from 'react';
import * as ReactDOM from 'react-dom';

import {App} from 'containers/App';

import './index.scss';

function startApp() {
  ReactDOM.render(
    <App
      name='Pixie Labs UI'
    />,
    document.getElementById('root'),
  );
}

startApp();
