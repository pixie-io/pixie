import {InMemoryCache} from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import {ApolloLink} from 'apollo-link';
import {setContext} from 'apollo-link-context';
import {onError} from 'apollo-link-error';
import {createHttpLink} from 'apollo-link-http';
import {ServerError} from 'apollo-link-http-common';
import gql from 'graphql-tag';
import * as RedirectUtils from 'utils/redirect-utils';
import {fetch} from 'whatwg-fetch';

// Apollo link that adds cookies in the request.
const cloudAuthLink = setContext((_, { headers }) => {
  return {
    headers: {
      ...headers,
      withCredentials: true,
    },
  };
});

// Apollo link that redirects to login page on HTTP status 401.
const loginRedirectLink = onError(({ networkError }) => {
  if (!!networkError && (networkError as ServerError).statusCode === 401) {
    const subdomain = window.location.host.split('.')[0];
    RedirectUtils.redirect('id', '/login', { ['site_name']: subdomain, ['no_cache']: 'true' });
  }
});

export const cloudGQLClient = new ApolloClient({
  cache: new InMemoryCache(),
  link: ApolloLink.from([
    cloudAuthLink,
    loginRedirectLink,
    createHttpLink({ uri: '/api/graphql', fetch }),
  ]),
});

interface GetClusterConnResults {
  clusterConnection: {
    ipAddress: string;
    token: string;
  };
}

const GET_CLUSTER_CONN = gql`
{
  clusterConnection {
    ipAddress
    token
  }
}`;

export function getClusterConnection(noCache: boolean = false) {
  return cloudGQLClient.query<GetClusterConnResults>({
    query: GET_CLUSTER_CONN,
    fetchPolicy: noCache ? 'network-only' : 'cache-first',
  }).then(({ data }) => data.clusterConnection);
}
