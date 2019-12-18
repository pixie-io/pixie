import './index.scss';

import {App} from 'containers/App';
import {SubdomainApp} from 'containers/subdomain-app';
import * as React from 'react';
import * as ReactDOM from 'react-dom';

function startApp() {
  if (document.getElementById('root')) {
    ReactDOM.render(
      <App />,
      document.getElementById('root'),
    );
  } else { // Should show subdomain app.
    ReactDOM.render(
      <SubdomainApp />,
      document.getElementById('subdomain-root'),
    );
  }
}

startApp();
