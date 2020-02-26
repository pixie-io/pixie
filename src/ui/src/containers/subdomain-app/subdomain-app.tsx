import './subdomain-app.scss';

import {getCloudGQLClient, getClusterConnection} from 'common/cloud-gql-client';
import {DARK_THEME} from 'common/mui-theme';
import {VizierGRPCClient} from 'common/vizier-grpc-client';
import ClientContext from 'common/vizier-grpc-client-context';
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
    vizierClient: null,
  };

  async componentDidMount() {
    const client = await getCloudGQLClient();
    const { ipAddress, token } = await getClusterConnection();
    const vizierClient = new VizierGRPCClient(ipAddress, token);
    this.setState({ client, vizierClient });
  }

  render() {
    const { client } = this.state;

    return !client ?
      <div>Loading...</div> :
      (
        <ThemeProvider theme={DARK_THEME}>
          <Router history={history}>
            <ApolloProvider client={client}>
              <ClientContext.Provider value={this.state.vizierClient}>
                <div style={{
                  height: '100vh',
                  width: '100vw',
                  display: 'flex',
                  flexDirection: 'column',
                  overflow: 'auto',
                }}>
                  <Vizier />
                </div>
              </ClientContext.Provider>
            </ApolloProvider>
          </Router>
          {!isProd() ? <VersionInfo /> : null}
        </ThemeProvider>
      );
  }
}
