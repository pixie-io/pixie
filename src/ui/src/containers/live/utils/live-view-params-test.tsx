import {LiveViewPage, matchLiveViewEntity, toEntityPathname} from './live-view-params';

describe('matchLiveViewEntity test', () => {
  it('should correctly match namespaces entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match a default view with no cluster', () => {
    const entity = matchLiveViewEntity('/live')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Default,
      params: {},
    });
  });

  it('should correctly match a default view with a cluster', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    });
  });
});

describe('toEntityPathname test', () => {
  it('should generate the url for the namespaces page', () => {
    const entity = {
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the url for the default page', () => {
    const entity = {
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar');
  });
});
