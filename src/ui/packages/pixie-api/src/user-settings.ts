export interface UserSettings {
  tourSeen: boolean;
}

/** Default values for every user setting, which will be returned from useSetting if there is not a stored value. */
export const DEFAULT_USER_SETTINGS: UserSettings = Object.freeze({
  tourSeen: false,
});
