import {Auth} from 'containers/Auth';
import {Home} from 'containers/Home';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';

export interface AppProps {
  name: string;
}

export class App extends React.Component<AppProps, any> {
  render() {
    return (
      <Router>
        <div>
          Pixie Labs UI!
          <Route exact path='/' component={Home} />
          <Route path='/login' component={Auth} />
        </div>
      </Router>
    );
  }
}

export default App;
