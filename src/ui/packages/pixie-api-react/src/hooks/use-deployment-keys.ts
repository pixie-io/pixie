import * as React from 'react';
import { useQuery, useMutation } from '@apollo/client/react';
import { DEPLOYMENT_KEY_QUERIES, GQLDeploymentKey } from '@pixie/api';
// noinspection ES6PreferShortImport
import { ImmutablePixieQueryResult } from '../utils/types';

interface DeploymentKeyHookResult {
  createDeploymentKey: () => Promise<string>;
  deleteDeploymentKey: (id: string) => Promise<void>;
  deploymentKeys: GQLDeploymentKey[];
}

/**
 * Retrieves a listing of active Pixie Deployment keys, as well as functions to create and delete keys.
 * Creating a key requires no arguments; deleting one requires the ID of the key to be deleted.
 *
 * Usage:
 * ```
 * const [{ createDeploymentKey, deleteDeploymentKey, deploymentKeys }, loading, error] = useDeploymentKeys();
 * ```
 */
export function useDeploymentKeys(): ImmutablePixieQueryResult<DeploymentKeyHookResult> {
  const { data, loading, error } = useQuery<{ deploymentKeys: GQLDeploymentKey[] }>(
    DEPLOYMENT_KEY_QUERIES.LIST_DEPLOYMENT_KEYS,
    { pollInterval: 2000 },
  );

  // Creation returns the ID of the created key and takes no arguments.
  const [creator] = useMutation<{ CreateDeploymentKey: { id: string } }, void>(
    DEPLOYMENT_KEY_QUERIES.CREATE_DEPLOYMENT_KEY);
  // Deletion returns a boolean stating whether it succeeded, and takes an ID to attempt to delete.
  const [deleter] = useMutation<void, { id: string }>(DEPLOYMENT_KEY_QUERIES.DELETE_DEPLOYMENT_KEY);

  // Waiting for state to settle ensures we don't change the result object's identity more than once.
  const resultIsStable = !loading && (error || data);

  // Abstracting away Apollo details, create resolves with the new ID string and delete accepts a string.
  // If an error occurs, it still falls through in the form of a rejected promise that .catch() will see.
  const result: DeploymentKeyHookResult = React.useMemo(() => ({
    createDeploymentKey: () => creator().then(({ data: { CreateDeploymentKey: { id } } }) => id),
    deleteDeploymentKey: (id: string) => deleter({ variables: { id } }).then(() => {}),
    deploymentKeys: loading || error || !data?.deploymentKeys ? undefined : data.deploymentKeys,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [resultIsStable]);

  // Forcing the result to only present once it's settled, to avoid bouncy definitions.
  return [result, loading, error];
}
