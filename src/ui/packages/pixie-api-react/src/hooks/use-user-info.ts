import { useQuery } from '@apollo/client/react';
import { USER_QUERIES, GQLUserInfo } from '@pixie/api';
import { ImmutablePixieQueryResult } from '../utils/types';

export function useUserInfo(): ImmutablePixieQueryResult<GQLUserInfo> {
  const { loading, data, error } = useQuery<{ user: GQLUserInfo }>(
    USER_QUERIES.GET_USER_INFO,
    { fetchPolicy: 'network-only' },
  );

  return [data?.user, loading, error];
}
