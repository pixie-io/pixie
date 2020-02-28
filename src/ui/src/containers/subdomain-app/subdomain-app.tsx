import './subdomain-app.scss';

import {getCloudGQLClient, getClusterConnection} from 'common/cloud-gql-client';
import {DARK_THEME} from 'common/mui-theme';
import {VizierGRPCClient} from 'common/vizier-grpc-client';
import {VersionInfo} from 'components/version-info/version-info';
import Vizier from 'containers/vizier';
import * as React from 'react';
import {ApolloProvider} from 'react-apollo';
import {Router} from 'react-router-dom';
import {isProd} from 'utils/env';
import history from 'utils/pl-history';

import {ThemeProvider} from '@material-ui/core/styles';

export class SubdomainApp extends React.Component {
  state = {
    client: null,
  };

  async componentDidMount() {
    const client = await getCloudGQLClient();
    this.setState({ client });
  }

  render() {
    const { client } = this.state;

    return !client ?
      <div>Loading...</div> :
      (
        <ThemeProvider theme={DARK_THEME}>
          <Router history={history}>
            <ApolloProvider client={client}>
              <div style={{
                height: '100vh',
                width: '100vw',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'auto',
              }}>
                <Vizier />
              </div>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </ThemeProvider>
      );
  }
}
