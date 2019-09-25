import * as RedirectUtils from './redirect-utils';

jest.mock('containers/constants', () => ({ DOMAIN_NAME: 'dev.withpixie.dev' }));

describe('RedirectUtils test', () => {
  it('should return correct url for no subdomain and no params', () => {
    expect(RedirectUtils.getRedirectPath('', '/vizier/query', {})).toEqual(
      'http://dev.withpixie.dev/vizier/query',
    );
  });

  it('should return correct url for subdomain and no params', () => {
    expect(RedirectUtils.getRedirectPath('test', '/vizier/query', {})).toEqual(
      'http://test.dev.withpixie.dev/vizier/query',
    );
  });

  it('should return correct url for subdomain and params', () => {
    expect(RedirectUtils.getRedirectPath('test', '/vizier/query', {test: 'abc', param: 'def'})).toEqual(
      'http://test.dev.withpixie.dev/vizier/query?test=abc&param=def',
    );
  });
});
