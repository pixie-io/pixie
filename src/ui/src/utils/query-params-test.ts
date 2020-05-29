import { QueryParams, Window } from './query-params';

describe('query params', () => {
  const mockWindow = {
    location: {
      search: '?script=px/script&foo=&bar=bar',
      protocol: 'https:',
      host: 'test',
      pathname: '/',
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
  });

  it('populates the scriptId and args from the url', () => {
    const instance = new QueryParams(mockWindow as Window);
    expect(instance.scriptId).toBe('px/script');
    expect(instance.args).toEqual({ foo: '', bar: 'bar' });
  });


  // TODO(malthus): The keys order might not be stable, so the path comparison might fail.
  describe('setArgs', () => {
    it('updates the URL', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setArgs({ what: 'now' });
      const expectedPath = 'https://test/?script=px%2Fscript&what=now';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('ignores the script and diff fields', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setArgs({ script: 'another one', what: 'now', diff: 'jjj' });
      const expectedPath = 'https://test/?script=px%2Fscript&what=now';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('setScript', () => {
    it('updates the url', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setScript('newScript', 'some changes');
      const expectedPath = 'https://test/?bar=bar&diff=some%20changes&foo=&script=newScript';
      expect(mockWindow.history.replaceState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('commitAll', () => {
    it('updates the url', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.commitAll('newScript', 'some changes', { fiz: 'biz' });
      const expectedPath = 'https://test/?diff=some%20changes&fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('omits the diff field if it is empty', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.commitAll('newScript', '', { fiz: 'biz' });
      const expectedPath = 'https://test/?fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('does not update the history stack if params are unchanged', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.commitAll('px/script', '', { foo: '', bar: 'bar' });
      expect(mockWindow.history.pushState).not.toHaveBeenCalled();
    });
  });

  describe('onChange', () => {
    it('emits itself when popstate event is fired', (done) => {
      const instance = new QueryParams(mockWindow as Window);
      const subscription = instance.onChange.subscribe((self) => {
        expect(self).toBe(instance);
        subscription.unsubscribe();
        done();
      });

      expect(mockWindow.addEventListener).toHaveBeenCalledTimes(1);
      const handler = mockWindow.addEventListener.mock.calls[0][1];
      handler();
    });
  });
});
