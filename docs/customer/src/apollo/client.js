import { InMemoryCache } from 'apollo-cache-inmemory';
import { ApolloClient } from 'apollo-client';
import { setContext } from 'apollo-link-context';
import { createHttpLink } from 'apollo-link-http';

const cloudFetch = (uri, options) => {
  // eslint-disable-next-line
  options.headers.withCredentials = true;
  return window.fetch(uri, options).then((resp) => {
    const result = {};
    result.ok = true;
    result.text = () => new Promise((resolve) => {
      resolve(resp.text());
    });
    return result;
  });
};

const cloudLink = createHttpLink({
  uri: '/api/graphql',
  fetch: cloudFetch,
});

const cloudAuthLink = setContext((_, { headers }) => ({
  headers: {
    ...headers,
    withCredentials: true,
  },
}));

const gqlCache = new InMemoryCache();

const cloudClient = new ApolloClient({
  cache: gqlCache,
  link: cloudAuthLink.concat(cloudLink),
});

export default cloudClient;
