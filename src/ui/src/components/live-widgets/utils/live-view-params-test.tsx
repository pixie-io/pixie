import { getLiveViewTitle, LiveViewPage, matchLiveViewEntity, toEntityPathname,
         toSingleEntityPage } from './live-view-params';
import { SemanticType } from 'types/generated/vizier_pb';

describe('matchLiveViewEntity test', () => {
  it('should correctly match the cluster page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match namespaces entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match namespace entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Namespace,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match nodes entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/nodes')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Nodes,
      params: {},
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match node entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/nodes/node-123')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Node,
      params: {
        node: 'node-123',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match pods entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Pods,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match pod entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods/orders-123')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Pod,
      params: {
        pod: 'px-sock-shop/orders-123',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match services entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Services,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should correctly match service entity page', () => {
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Service,
      params: {
        service: 'px-sock-shop/orders',
      },
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
    const entity = matchLiveViewEntity('/live/clusters/gke%3Afoobar/script')
    expect(entity).toStrictEqual({
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    });
  });
});

describe('toEntityPathname test', () => {
  it('should generate the url for the cluster page', () => {
    const entity = {
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar');
  });

  it('should generate the url for the namespaces page', () => {
    const entity = {
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the url for the namespace page', () => {
    const entity = {
      page: LiveViewPage.Namespace,
      params: {
        namespace: 'px-sock-shop'
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop');
  });

  it('should generate the url for the nodes page', () => {
    const entity = {
      page: LiveViewPage.Nodes,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/nodes');
  });

  it('should generate the url for the node page', () => {
    const entity = {
      page: LiveViewPage.Node,
      params: {
        node: 'node-123'
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/nodes/node-123');
  });

  it('should generate the url for the pods page', () => {
    const entity = {
      page: LiveViewPage.Pods,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods');
  });

  it('should generate the url for the pod page', () => {
    const entity = {
      page: LiveViewPage.Pod,
      params: {
        pod: 'px-sock-shop/orders-123',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods/orders-123');
  });

  it('should generate the url for the services page', () => {
    const entity = {
      page: LiveViewPage.Services,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services');
  });

  it('should generate the url for the service page', () => {
    const entity = {
      page: LiveViewPage.Service,
      params: {
        service: 'px-sock-shop/orders',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders');
  });

  it('should generate the url for the default page', () => {
    const entity = {
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity)).toEqual('/live/clusters/gke%3Afoobar/script');
  });
});

describe('toSingleEntityPage test', () => {
  it('should generate the entity for a namespace', () => {
    const entity = toSingleEntityPage('px-sock-shop', SemanticType.ST_NAMESPACE_NAME, 'gke:foobar');
    expect(entity).toStrictEqual({
      page: LiveViewPage.Namespace,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should generate the entity for a node', () => {
    const entity = toSingleEntityPage('node-123', SemanticType.ST_NODE_NAME, 'gke:foobar');
    expect(entity).toStrictEqual({
      page: LiveViewPage.Node,
      params: {
        node: 'node-123',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should generate the entity for a pod', () => {
    const entity = toSingleEntityPage('px-sock-shop/orders-123', SemanticType.ST_POD_NAME, 'gke:foobar');
    expect(entity).toStrictEqual({
      page: LiveViewPage.Pod,
      params: {
        pod: 'px-sock-shop/orders-123',
      },
      clusterName: 'gke:foobar',
    });
  });

  it('should generate the entity for a service', () => {
    const entity = toSingleEntityPage('px-sock-shop/orders', SemanticType.ST_SERVICE_NAME, 'gke:foobar');
    expect(entity).toStrictEqual({
      page: LiveViewPage.Service,
      params: {
        service: 'px-sock-shop/orders',
      },
      clusterName: 'gke:foobar',
    });
  });
});

describe('getLiveViewTitle test', () => {
  it('should generate the title for the cluster page', () => {
    const page = LiveViewPage.Cluster;
    const params = {};
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('my default title');
  });

  it('should generate the title for the namespaces page', () => {
    const page = LiveViewPage.Namespaces;
    const params = {};
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('namespaces');
  });

  it('should generate the title for the namespace page', () => {
    const page = LiveViewPage.Namespace;
    const params = {
      namespace: 'px-sock-shop'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('px-sock-shop');
  });

  it('should generate the title for the nodes page', () => {
    const page = LiveViewPage.Nodes;
    const params = {};
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('nodes');
  });

  it('should generate the title for the node page', () => {
    const page = LiveViewPage.Node;
    const params = {
      node: 'node-123'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('node-123');
  });

  it('should generate the title for the pods page', () => {
    const page = LiveViewPage.Pods;
    const params = {
      namespace: 'px-sock-shop'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('px-sock-shop/pods');
  });

  it('should generate the title for the pod page', () => {
    const page = LiveViewPage.Pod;
    const params = {
      pod: 'px-sock-shop/orders-123'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('px-sock-shop/orders-123');
  });

  it('should generate the title for the services page', () => {
    const page = LiveViewPage.Services;
    const params = {
      namespace: 'px-sock-shop'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('px-sock-shop/services');
  });

  it('should generate the title for the service page', () => {
    const page = LiveViewPage.Service;
    const params = {
      service: 'px-sock-shop/orders'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('px-sock-shop/orders');
  });

  it('should generate the title for the default page', () => {
    const page = LiveViewPage.Default;
    const params = {
      namespace: 'do-not-use'
    };
    const defaultTitle = 'my default title';
    expect(getLiveViewTitle(defaultTitle, page, params)).toEqual('my default title');
  });
});
