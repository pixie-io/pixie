type Action = string;

export type Handlers<T extends Action> = {
  [action in T]: () => void;
};

export type KeyMap<T extends Action> = {
  [action in T]: {
    sequence: string | string[];
    displaySequence: string | string[];
    description: string;
  }
};

export type ShortcutsContextProps<T extends string> = {
  [action in T]: KeyMap<T>[action] & { handler: Handlers<T>[action] };
};
