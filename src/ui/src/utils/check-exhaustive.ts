/**
 * Use in the default of a switch/case block to prove that all values
 * have been considered. Fails to compile if any were forgotten at compile
 * time, and throws if an unexpected value is encountered at runtime.
 *
 * @param val The value that is being switch/cased
 */
export function checkExhaustive(val: never): never {
  throw new Error(`Unexpected value: ${JSON.stringify(val)}`);
}
