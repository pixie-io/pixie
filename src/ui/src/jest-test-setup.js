// Setup mock object for local storage.
var localStorageMock = (() => {
  var store = {};
  return {
    getItem: (key) => {
      return store[key];
    },
    setItem: (key, value) => {
      store[key] = value.toString();
    },
    clear: () => {
      store = {};
    },
    removeItem: (key) => {
      delete store[key];
    },
  };
})();

var analyticsMock = (() => {
  return {
    page: () => { return; },
  };
})();

Object.defineProperty(window, 'localStorage', {value: localStorageMock});
Object.defineProperty(window, 'analytics', {value: analyticsMock});

// This is a hack to get clsx to actually work in the test.
// The main issue is that in our actual code clsx is imported as an es module:
// import clsx from 'clsx';
// However, jest doesn't seem to care and imports it as commonjs module, hence
// we would get the error: clsx_1.default is not a function.
// This is because jest ignores the "module" option in clsx package option.
// https://github.com/facebook/jest/issues/2702
jest.mock('clsx', () => ({default: jest.requireActual('clsx')}));
