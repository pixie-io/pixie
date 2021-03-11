import * as React from 'react';
import { PixieAPIContext } from 'api-context';

/**
 * Checks if the current session has a valid bearer token to use Pixie's API, by requesting the `authorized` endpoint.
 * `authorized` responds with no body, using the HTTP status code to answer. As simple as it gets.
 */
export function useIsAuthenticated(): { loading: boolean; authenticated: boolean; error?: any } {
  const [promise, setPromise] = React.useState<Promise<boolean>>(null);
  // Using an object instead of separate variables because using multiple setState does NOT batch if it happens outside
  // of React's scope (like resolved promises or Observable subscriptions). To make it atomic, have to use ONE setState.
  const [{ loading, authenticated, error }, setState] = React.useState({
    loading: true, authenticated: false, error: undefined,
  });

  const client = React.useContext(PixieAPIContext);
  React.useEffect(() => {
    if (client) {
      const p = fetch(`${client.options.uri}/authorized`).then((response) => response.status === 200);
      setPromise(p);
    } else {
      throw new Error('useIsAuthenticated requires your component to be within a PixieAPIContextProvider to work!');
    }
  }, [setPromise, client]);

  React.useEffect(() => {
    if (!promise) return;
    setState({ loading: true, authenticated, error: undefined });
    promise.then((isAuthenticated) => {
      setState({ loading: false, authenticated: isAuthenticated, error: undefined });
    }).catch((e) => {
      setState({ loading: false, authenticated: false, error: e });
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [promise]);

  return { authenticated, loading, error };
}
