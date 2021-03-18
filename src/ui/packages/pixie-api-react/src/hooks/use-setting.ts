import * as React from 'react';
import { UserSettings, DEFAULT_USER_SETTINGS, USER_QUERIES } from '@pixie-labs/api';
import { useMutation, useQuery } from '@apollo/client/react';
// noinspection ES6PreferShortImport
import { MutablePixieQueryGuaranteedResult, SetStateFunc } from '../utils/types';

type TData = { userSettings: Array<Record<string, string>> };

/**
 * Provides the value and a setter for a given (recognized) user setting.
 * If the user has not set that setting before or it's still loading, a default value will be provided in its stead.
 *
 * Usage:
 * ```
 * // The tourSeen setting is a boolean (and is typed as such); other settings have other types.
 * const [tourSeen, setTourSeen, loading, error] = useSetting('tourSeen');
 * ```
 */
export function useSetting(
  key: keyof UserSettings,
): MutablePixieQueryGuaranteedResult<UserSettings[typeof key]> {
  type ValueType = UserSettings[typeof key];

  const [setting, setSetting] = React.useState<ValueType>(DEFAULT_USER_SETTINGS[key]);
  const [storeSetting] = useMutation(USER_QUERIES.SAVE_USER_SETTING);
  const { loading, data, error } = useQuery<TData>(
    USER_QUERIES.GET_ALL_USER_SETTINGS,
    { fetchPolicy: 'cache-and-network' },
  );

  // Upstream, settings are stored as strings no matter what they contain. Since the data type is not stored there, we
  // make some compromises at runtime when trying to parse them out.
  let stored: ValueType|undefined;
  if (!loading && !error) {
    const storedString = data.userSettings.find((s) => s.key === key)?.value;
    try {
      // Compromise 1: Strings that happen to look like valid JSON (for example, 'false') are treated as such
      stored = JSON.parse(storedString);
    } catch {
      // Compromise 2: Malformed data gets treated as a string even if it isn't supposed to be
      stored = storedString as any as ValueType;
    }
  }

  const setter: SetStateFunc<ValueType> = (actionOrValue) => {
    const value: ValueType = typeof actionOrValue === 'function'
      ? actionOrValue(setting)
      : actionOrValue;
    if (value !== stored) {
      setSetting(value);
      storeSetting({ variables: { key, value: JSON.stringify(value) } }).then();
    }
  };

  // Triggered when the initial state finishes loading, and (redundantly) when a setting is updated in Apollo's cache.
  React.useEffect(() => {
    if (!loading && !error && stored !== setting && stored !== undefined) {
      setSetting(stored);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [loading, stored]);

  // The loading state becomes false before the stored and state values are synchronized. From the consumer's viewpoint,
  // that intermediate state should not be visible. Instead, they should see both values update at the same time.
  return [setting, setter, loading || setting !== stored, error];
}
