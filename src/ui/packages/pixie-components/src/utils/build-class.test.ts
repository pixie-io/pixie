import { buildClass } from 'utils/build-class';

describe('className builder', () => {
  it('Processes empty scenarios', () => {
    expect(buildClass()).toBe('');
    expect(buildClass(null, false, undefined, true, {}, [[{}]])).toBe('');
  });

  it('Processes basic string lists', () => {
    expect(buildClass('a', 'b', 'c')).toBe('a b c');
  });

  it('Processes sparse lists with falsy values', () => {
    expect(buildClass('a', undefined, 'b', false, 'c')).toBe('a b c');
  });

  it('Processes nested lists', () => {
    expect(buildClass('a', ['b', ['c']])).toBe('a b c');
  });

  it('Processes objects', () => {
    expect(buildClass({
      a: true,
      b: false,
      c: 'truthy',
      d: null,
    })).toBe('a c');
  });

  it('Processes unreasonable requests reasonably', () => {
    expect(buildClass(
      'a',
      false,
      'b',
      [
        'c',
        { d: 'truthy value' },
      ],
      undefined,
      null,
      '',
      'e')).toBe('a b c d e');
  });

  it('Throws when given unexpected JS types', () => {
    expect(() => buildClass(1)).toThrow();
    expect(() => buildClass(() => {})).toThrow();
  });
});
