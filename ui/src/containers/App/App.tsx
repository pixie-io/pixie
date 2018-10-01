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
          <Route exact path='/' render={(props) => <Home name={this.props.name} {...props} />} />
        </div>
      </Router>
    );
  }
}

export default App;
