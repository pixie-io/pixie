import * as React from 'react';
import {Route} from 'react-router-dom';

import {CompanyCreate, CompanyLogin} from './company-login';

import './login.scss';

interface LoginProps {
  match: any;
}
export class Login extends React.Component<LoginProps, {}> {
  constructor(props) {
    super(props);
  }

  render() {
    const matchPath = this.props.match.path;
    return (
      <div className='login'>
        <div className='login-body'>
            <Route exact path={`/`} component={CompanyLogin} />
            <Route exact path={`/create`} component={CompanyCreate} />
        </div>
      </div>
    );
  }
}
export default Login;
