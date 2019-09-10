import * as React from 'react';
import * as ReactDOM from 'react-dom';

import {App} from 'containers/App';
import {SubdomainApp} from 'containers/subdomain-app';

import './index.scss';

function startApp() {
  if (document.getElementById('root')) {
      ReactDOM.render(
        <App
          name='Pixie Labs UI'
        />,
        document.getElementById('root'),
      );
  } else { // Should show subdomain app.
      ReactDOM.render(
        <SubdomainApp
          name='Pixie Labs UI'
        />,
        document.getElementById('subdomain-root'),
      );
  }
}

startApp();
