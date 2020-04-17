import './index.scss';
// tslint:disable-next-line: ordered-imports
import 'segment';

import {App} from 'containers/App';
import * as React from 'react';
import * as ReactDOM from 'react-dom';
import {configure} from 'react-hotkeys';

configure({
  // React hotkeys defaults to ignore events from within ['input', 'select', 'textarea'].
  // We want the Pixie command to work from anywhere.
  ignoreTags: ['select'],
});

ReactDOM.render(<App />, document.getElementById('root'));
