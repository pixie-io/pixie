import * as React from 'react';
import {Route, Switch} from 'react-router-dom';

import {AuthSuccess} from './auth-success';
import {CompanyCreate, CompanyLogin} from './company-login';
import {Logout} from './logout';
import {UserCreate, UserLogin} from './user-login';

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
            <Switch>
              <Route exact path={`/create`} component={CompanyCreate} />
              <Route exact path={`/login`} component={UserLogin} />
              <Route exact path={`/create-site`} component={UserCreate} />
              <Route exact path={`/logout`} component={Logout} />
              <Route exact path={`/auth_success`} component={AuthSuccess} />
              <Route path={`/`} component={CompanyLogin} />
            </Switch>
        </div>
      </div>
    );
  }
}
export default Login;
