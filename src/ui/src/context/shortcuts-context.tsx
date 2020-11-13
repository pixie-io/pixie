type Action = string;

export type Handlers<T extends Action, E extends UIEvent = KeyboardEvent> = {
  [action in T]: (e?: E) => void;
};

export type KeyMap<T extends Action> = {
  [action in T]: {
    sequence: string;
    displaySequence: string | string[];
    description: string;
  }
};

export type ShortcutsContextProps<T extends Action> = {
  [action in T]: KeyMap<T>[action] & { handler: Handlers<T>[action] };
};
