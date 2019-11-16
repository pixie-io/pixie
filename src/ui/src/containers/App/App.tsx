import './App.scss';

import {cloudGQLClient} from 'common/cloud-gql-client';
import {Login} from 'containers/login';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {BrowserRouter as Router, Link, Route, Switch} from 'react-router-dom';

export interface AppProps {
  name: string;
}

export class App extends React.Component<AppProps, {}> {
  render() {
    return (
      <Router>
        <ApolloProvider client={cloudGQLClient}>
          <div className='main-page'>
            <div className='content'>
              <Switch>
                <Route exact path='/create' component={Login} />
                <Route exact path='/auth_success' component={Login} />
                <Route component={Login} />
              </Switch>
            </div>
          </div>
        </ApolloProvider>
      </Router>
    );
  }
}

export default App;
