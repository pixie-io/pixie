import {Auth} from 'containers/Auth';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';

export interface AppProps {
  name: string;
}

export class Home extends React.Component<AppProps, any> {
  auth: Auth;
  constructor(props) {
    super(props);

    this.auth = new Auth({});
  }

  login = () => {
    this.auth.login();
  }

  logout = () => {
    this.auth.logout();
  }

  render() {
    const { isAuthenticated } = this.auth;
    return (
      <div>
        {this.props.name}
        <p/>
          {
            !isAuthenticated() && (
              <a
                href='#'
                onClick={this.login}
              >
                Log In
              </a>
            )
          }
          {
            isAuthenticated() && (
              <a
                href='#'
                onClick={this.logout}
              >
                Log Out
              </a>
            )
          }
      </div>
    );
  }
}

export default Home;
