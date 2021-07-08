/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import { SemanticType } from 'app/types/generated/vizierapi_pb';
import {
  entityPageForScriptId, LiveViewPage,
  scriptToEntityURL, toEntityPathname, toEntityURL,
  toSingleEntityPage,
} from './live-view-params';

const noEmbed = {
  isEmbedded: false,
  disableTimePicker: false,
  widget: null,
};

describe('toEntityPathname test', () => {
  it('should generate the pathname for the cluster page', () => {
    const entity = {
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar');
  });

  it('should generate the pathname for the namespaces page', () => {
    const entity = {
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the pathname for the namespace page', () => {
    const entity = {
      page: LiveViewPage.Namespace,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop');
  });

  it('should generate the pathname for the nodes page', () => {
    const entity = {
      page: LiveViewPage.Nodes,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/nodes');
  });

  it('should generate the pathname for the node page', () => {
    const entity = {
      page: LiveViewPage.Node,
      params: {
        node: 'node-123',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/nodes/node-123');
  });

  it('should generate the pathname for the pods page', () => {
    const entity = {
      page: LiveViewPage.Pods,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods');
  });

  it('should generate the pathname for the pod page', () => {
    const entity = {
      page: LiveViewPage.Pod,
      params: {
        pod: 'px-sock-shop/orders-123',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual(
      '/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods/orders-123',
    );
  });

  it('should generate the pathname for the services page', () => {
    const entity = {
      page: LiveViewPage.Services,
      params: {
        namespace: 'px-sock-shop',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services');
  });

  it('should generate the pathname for the service page', () => {
    const entity = {
      page: LiveViewPage.Service,
      params: {
        service: 'px-sock-shop/orders',
      },
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual(
      '/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders',
    );
  });

  it('should generate the pathname for the default page', () => {
    const entity = {
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityPathname(entity, false)).toEqual('/live/clusters/gke%3Afoobar');
  });
});

describe('toEntityURL test', () => {
  it('should generate the url for an entity page ', () => {
    const entity = {
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, noEmbed, {
      propagatedParam: 'foo',
    })).toEqual('/live/clusters/gke%3Afoobar?propagatedParam=foo');
  });

  it('should generate the url an entity page with no params', () => {
    const entity = {
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, noEmbed)).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the url an entity page with empty params', () => {
    const entity = {
      page: LiveViewPage.Namespaces,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, noEmbed, {})).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the url for a non-entity page ', () => {
    const entity = {
      page: LiveViewPage.Default,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, noEmbed, {
      propagatedParam: 'foo',
    })).toEqual('/live/clusters/gke%3Afoobar?propagatedParam=foo');
  });

  it('should generate the url for an entity page with widget', () => {
    const entity = {
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, {
      isEmbedded: true,
      widget: 'foo',
      disableTimePicker: false,
    }, {
      propagatedParam: 'foo',
    })).toEqual('/embed/live/clusters/gke%3Afoobar?propagatedParam=foo&widget=foo');
  });

  it('should generate the url for an entity page with disableTimePicker', () => {
    const entity = {
      page: LiveViewPage.Cluster,
      params: {},
      clusterName: 'gke:foobar',
    };
    expect(toEntityURL(entity, {
      isEmbedded: true,
      disableTimePicker: true,
      widget: null,
    }, {
      propagatedParam: 'foo',
    })).toEqual('/embed/live/clusters/gke%3Afoobar?disable_time_picker=true&propagatedParam=foo');
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

describe('entityPageForScriptId', () => {
  it('should return the right enum for an entity script id', () => {
    expect(entityPageForScriptId('px/cluster')).toEqual(LiveViewPage.Cluster);
  });

  it('should return the right enum for a non-entity script id', () => {
    expect(entityPageForScriptId('px/http_data')).toEqual(LiveViewPage.Default);
  });
});

describe('scriptToEntityURL', () => {
  it('should return an entity URL for an entity script', () => {
    expect(scriptToEntityURL('px/namespace', 'aClusterName', noEmbed, {
      namespace: 'foobar',
      anotherArg: '-30s',
    })).toEqual('/live/clusters/aClusterName/namespaces/foobar?anotherArg=-30s');
  });

  it('should return a non entity URL for a non entity script', () => {
    expect(scriptToEntityURL('px/http_data', 'aClusterName', noEmbed, {
      namespace: 'foobar',
      anotherArg: '-30s',
    })).toEqual('/live/clusters/aClusterName?anotherArg=-30s&namespace=foobar&script=px%2Fhttp_data');
  });
});
