import * as React from 'react';

export const LIVE_VIEW_DATA_DRAWER_OPENED_KEY = 'px-live-data-drawer-opened';
export const LIVE_VIEW_EDITOR_OPENED_KEY = 'px-live-editor-opened';
export const LIVE_VIEW_PIXIE_SCRIPT_KEY = 'px-live-pixie-script';
export const LIVE_VIEW_SCRIPT_ID_KEY = 'px-live-script-id';
export const LIVE_VIEW_VIS_SPEC_KEY = 'px-live-vis';
export const LIVE_VIEW_EDITOR_SPLITS_KEY = 'px-live-editor-splits';
export const LIVE_VIEW_DATA_DRAWER_SPLITS_KEY = 'px-live-data-drawer-splits';
export const LIVE_VIEW_SCRIPT_ARGS_KEY = 'px-live-script-args';
export const CLUSTER_ID_KEY = 'px-cluster-id';

type StorageKey =
  typeof LIVE_VIEW_DATA_DRAWER_OPENED_KEY |
  typeof LIVE_VIEW_DATA_DRAWER_OPENED_KEY |
  typeof LIVE_VIEW_EDITOR_OPENED_KEY |
  typeof LIVE_VIEW_PIXIE_SCRIPT_KEY |
  typeof LIVE_VIEW_SCRIPT_ID_KEY |
  typeof LIVE_VIEW_VIS_SPEC_KEY |
  typeof LIVE_VIEW_EDITOR_SPLITS_KEY |
  typeof LIVE_VIEW_DATA_DRAWER_SPLITS_KEY |
  typeof LIVE_VIEW_SCRIPT_ARGS_KEY |
  typeof CLUSTER_ID_KEY;

interface KeyStore {
  getItem(key: string): string;
  setItem(key: string, value: string): void;
}

export function useStorage<T>(store: KeyStore, key: StorageKey, initialValue?: T):
[T, React.Dispatch<React.SetStateAction<T>>] {
  const [state, setState] = React.useState<T>(() => {
    try {
      const stored = store.getItem(key);
      if (stored) {
        return JSON.parse(stored);
      }
    } catch (e) {
      //
    }
    return initialValue;
  });

  // Update the state in the store on changes.
  React.useEffect(() => {
    store.setItem(key, JSON.stringify(state));
  }, [state, store, key]);

  return [state, setState];
}

export function useLocalStorage<T>(key: StorageKey, initialValue?: T):
[T, React.Dispatch<React.SetStateAction<T>>] {
  return useStorage(localStorage, key, initialValue);
}

// Hook to use sessionStorage. It saves to both localStorage as well as sessionStorage,
// and it attempts to restore from sessionStorage first, and defaults to localstorage
// (on first load).
export function useSessionStorage<T>(key: StorageKey, initialValue?: T):
[T, React.Dispatch<React.SetStateAction<T>>] {
  const setItem = (key: string, value: string) => {
    localStorage.setItem(key, value);
    sessionStorage.setItem(key, value);
  };
  const getItem = (key: string): string => {
    const value = sessionStorage.getItem(key);
    if (value === null) {
      return localStorage.getItem(key);
    }
    return value;
  };
  return useStorage({ setItem, getItem }, key, initialValue);
}
