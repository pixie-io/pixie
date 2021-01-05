/**
 * Utility function to create a valid CSS className from a potentially sparse list of classes.
 * Typical uses might look like these:
 * ```
 * buildClass({
 *   foo: true,
 *   bar: false,
 *   baz: true,
 * }) // returns 'foo baz'
 *
 * // returns 'classA classC' if someCondition is not satisfied. Returns 'classA classB classC' if it is.
 * buildClass('classA', someCondition && 'classB', 'classC')
 * ```
 *
 * It can also handle more exotic calls (please don't actually do this):
 * ```
 * // returns 'a b c d e'
 * buildClass('a', false, 'b', ['c', {d: 'truthy value'}], undefined, null, '', 'e')
 * ```
 * @param classes
 */
export function buildClass(...classes: any[]): string {
  return classes
    .flat(Infinity)
    .map((c) => {
      switch (typeof c) {
        case 'object':
          // typeof null === 'object', and Object.keys(null) throws an error, so we have to check for it.
          return c ? buildClass(...Object.keys(c).filter((k) => c[k])) : '';
        case 'undefined':
        case 'boolean':
          // Boolean true doesn't make sense, so treat it the same as false/undefined/empty string.
          return '';
        case 'string':
          return c;
        case 'bigint':
        case 'number':
        case 'function':
        case 'symbol':
        default:
          throw new Error(`Cannot create a classname from a ${typeof c}`);
      }
    })
    .filter((c) => c)
    .join(' ');
}
