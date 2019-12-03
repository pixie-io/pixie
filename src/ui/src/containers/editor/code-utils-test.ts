import {getCodeFromStorage, saveCodeToStorage} from './code-utils';

describe('save code utils', () => {
  it('stores and retrieves code correctly', () => {
    const code = '# some complicated code';
    const id = 'one of a kind id';
    saveCodeToStorage(id, code);
    expect(getCodeFromStorage(id)).toBe(code);
  });
});
