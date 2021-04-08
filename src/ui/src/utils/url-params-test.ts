import { take } from 'rxjs/operators';

import { URLParams, Window } from './url-params';

describe('url params', () => {
  const mockWindow = {
    location: {
      search: '?script=px/script&foo=&bar=bar',
      protocol: 'https:',
      host: 'test',
      pathname: '/pathname',
    },
    history: {
      pushState: jest.fn(),
      replaceState: jest.fn(),
    },
    addEventListener: jest.fn(),
    removeEventListener: jest.fn(),
  };

  beforeEach(() => {
    mockWindow.history.pushState.mockClear();
    mockWindow.history.replaceState.mockClear();
    mockWindow.addEventListener.mockClear();
  });

  it('populates the scriptId and args from the url', () => {
    const instance = new URLParams(mockWindow as Window);
    expect(instance.scriptId).toBe('px/script');
    expect(instance.args).toEqual({ foo: '', bar: 'bar' });
  });

  describe('setArgs', () => {
    it('updates the URL', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.setArgs({ what: 'now' });
      const expectedPath = 'https://test/pathname?script=px%2Fscript&what=now';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('ignores the script and diff fields', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.setArgs({ script: 'another one', what: 'now', diff: 'jjj' });
      const expectedPath = 'https://test/pathname?script=px%2Fscript&what=now';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('setScript', () => {
    it('updates the url', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.setScript('newScript', 'some changes');
      const expectedPath = 'https://test/pathname?bar=bar&diff=some%20changes&foo=&script=newScript';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('commitAll', () => {
    it('updates the url', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.commitAll('newScript', 'some changes', { fiz: 'biz' });
      const expectedPath = 'https://test/pathname?diff=some%20changes&fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('omits the diff field if it is empty', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.commitAll('newScript', '', { fiz: 'biz' });
      const expectedPath = 'https://test/pathname?fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('does not update the history stack if params are unchanged', () => {
      const instance = new URLParams(mockWindow as Window);
      instance.commitAll('px/script', '', { foo: '', bar: 'bar' });
      expect(mockWindow.history.pushState).not.toHaveBeenCalled();
    });
  });

  describe('onChange', () => {
    it('emits itself the first time', (done) => {
      const instance = new URLParams(mockWindow as Window);
      instance.onChange
        .pipe(take(1))
        .subscribe((newParams) => {
          expect(newParams.scriptId).toBe(instance.scriptId);
          expect(newParams.scriptDiff).toBe(instance.scriptDiff);
          expect(newParams.args).toEqual(instance.args);
        }, done.fail, done);
    });

    it('emits itself when triggerOnChange is executed', (done) => {
      const instance = new URLParams(mockWindow as Window);
      instance.onChange
        .pipe(take(2)) // completes after the first 2 values are emitted.
        .subscribe((newParams) => {
          expect(newParams.scriptId).toBe(instance.scriptId);
          expect(newParams.scriptDiff).toBe(instance.scriptDiff);
          expect(newParams.args).toEqual(instance.args);
        }, done.fail, done);
      instance.triggerOnChange();
    });
  });
});
