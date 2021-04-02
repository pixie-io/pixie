// This type finds the names of each method on a class, as well as the parameters of those methods.
export type Invocation<Class> = {
  [Key in keyof Class]: Class[Key] extends (...args: infer Params) => any
    ? (Params extends [] ? [Key] : any[]) // TODO(nick,PC-819): TS 4.x lets us change this line to `[Key, ...Params]`.
    : never;
}[keyof Class];
