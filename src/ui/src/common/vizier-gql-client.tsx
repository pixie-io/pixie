import {InMemoryCache} from 'apollo-cache-inmemory';
import {ApolloClient} from 'apollo-client';
import {ApolloLink, fromPromise} from 'apollo-link';
import {setContext} from 'apollo-link-context';
import {onError} from 'apollo-link-error';
import {createHttpLink} from 'apollo-link-http';
import fetchWithTimeout from 'utils/fetch-timeout';

import {getClusterConnection} from './cloud-gql-client';

const TIMEOUT_MS = 5000; // Timeout after 5 seconds.
const LOCALHOST_ADDR = 'http://127.0.0.1:31067';
// TODO(malthus): Move these keys and localstorage access to a different file to avoid collision.
const VIZIER_AUTH_KEY = 'vizierAuth';
const VIZIER_MODE_KEY = 'vizierMode';

const gqlCache = new InMemoryCache();

interface VizierAuth {
  token: string;
  ipAddress: string;
}

type VizierMode = 'vizier' | 'proxy' | '';

class VizierAuthLink {
  private vizierMode: VizierMode = 'vizier';

  constructor() {
    const mode = localStorage.getItem(VIZIER_MODE_KEY);
    if (mode === 'vizier' || mode === 'proxy') {
      this.vizierMode = mode;
    }
  }

  getLink(): ApolloLink {
    return ApolloLink.from([
      this.withAuthLink,
      this.retryVizierModeLink,
      this.retryWithAuthLink,
      this.saveVizierModeLink,
      this.withVizierAddrLink,
      createHttpLink({ fetch: fetchWithTimeout(TIMEOUT_MS) }),
    ]);
  }

  private get retryWithAuthLink(): ApolloLink {
    return onError(({ networkError, operation, forward }) => {
      if (!networkError) {
        // The request worked, continue.
        return;
      }
      // Try Refreshing the token. This also updates the address.
      return fromPromise(this.getAuth(true))
        .flatMap((auth) => {
          const oldHeaders = operation.getContext().headers;
          operation.setContext({
            vizierAddr: auth.ipAddress,
            headers: {
              ...oldHeaders,
              authorization: `Bearer ${auth.token}`,
            },
          });
          return forward(operation);
        });
    });
  }

  private get retryVizierModeLink(): ApolloLink {
    return onError(({ networkError, operation, forward }) => {
      if (!networkError) {
        // The request worked, continue.
        return;
      }
      // Try a different vizier mode.
      const oldCtx = operation.getContext();
      const mode = oldCtx.mode === 'vizier' ? 'proxy' : 'vizier';
      operation.setContext({
        ...oldCtx,
        mode,
      });
      return forward(operation);
    });
  }

  private get saveVizierModeLink(): ApolloLink {
    return new ApolloLink((operation, forward) => forward(operation)
      .map((response) => {
        // If the operation succeeded, save the mode that was used.
        this.mode = operation.getContext().mode;
        return response;
      }));
  }

  private get withAuthLink(): ApolloLink {
    return setContext((_, { headers }) => {
      return this.getAuth().then((auth) => {
        return {
          mode: this.vizierMode,
          vizierAddr: auth.ipAddress,
          headers: {
            ...headers,
            authorization: `Bearer ${auth.token}`,
          },
        };
      });
    });
  }

  private get withVizierAddrLink(): ApolloLink {
    return setContext((_, context) => {
      const addr = context.mode === 'vizier' ? context.vizierAddr : LOCALHOST_ADDR;
      return {
        ...context,
        uri: `${addr}/graphql`,
      };
    });
  }

  private getAuth(noCache?: boolean): Promise<VizierAuth> {
    return getClusterConnection(noCache);
  }

  private set mode(mode: VizierMode) {
    this.vizierMode = mode;
    localStorage.setItem(VIZIER_MODE_KEY, mode);
  }
}

export const vizierGQLClient = new ApolloClient({
  cache: gqlCache,
  link: new VizierAuthLink().getLink(),
});
