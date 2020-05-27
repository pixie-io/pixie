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
    },
  };

  beforeEach(() => {
    mockWindow.history.pushState.mockClear();
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
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('ignores the script and diff fields', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setArgs({ script: 'another one', what: 'now', diff: 'jjj' });
      const expectedPath = 'https://test/?script=px%2Fscript&what=now';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('setScript', () => {
    it('updates the url', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setScript('newScript', 'some changes');
      const expectedPath = 'https://test/?bar=bar&diff=some%20changes&foo=&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });

  describe('setAll', () => {
    it('updates the url', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setAll('newScript', 'some changes', { fiz: 'biz' });
      const expectedPath = 'https://test/?diff=some%20changes&fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });

    it('omits the diff field if it is empty', () => {
      const instance = new QueryParams(mockWindow as Window);
      instance.setAll('newScript', '', { fiz: 'biz' });
      const expectedPath = 'https://test/?fiz=biz&script=newScript';
      expect(mockWindow.history.pushState).toBeCalledWith({ path: expectedPath }, '', expectedPath);
    });
  });
});
