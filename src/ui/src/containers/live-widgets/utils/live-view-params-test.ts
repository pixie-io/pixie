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
  deepLinkURLFromScript, deepLinkURLFromSemanticType,
} from './live-view-params';

const noEmbed = {
  disableTimePicker: false,
  widget: null,
};

const withEmbed = {
  disableTimePicker: true,
  widget: null,
};

const startTimeArgs = {
  start_time: '-7m',
};

// Note: the functions this file tests don't actually check embed state (anymore), but a top-level redirect
// does and expects consistency.
function fakeEmbed() {
  if (globalThis.top === globalThis.self) {
    delete globalThis.top;
    globalThis.top = {} as any;
  }
}

// Jest doesn't restore this on its own, so clean up there
function restoreEmbed() {
  if (globalThis.top !== globalThis.self) {
    delete globalThis.top;
    globalThis.top = globalThis.self;
  }
}

describe('deepLinkURLFromScript test', () => {
  beforeEach(restoreEmbed);
  afterEach(restoreEmbed);

  it('should generate the URL for the cluster page', () => {
    const url = deepLinkURLFromScript('px/cluster', 'gke:foobar', noEmbed, startTimeArgs);
    expect(url).toEqual('/live/clusters/gke%3Afoobar?start_time=-7m');
  });

  it('should generate the URL for the namespaces page (with empty args)', () => {
    const url = deepLinkURLFromScript('px/namespaces', 'gke:foobar', noEmbed, null);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces');
  });

  it('should generate the URL for the namespace page (with embed)', () => {
    fakeEmbed();
    const url = deepLinkURLFromScript('px/namespace', 'gke:foobar', withEmbed, {
      namespace: 'foobar',
    });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/foobar?disable_time_picker=true');
  });

  it('should generate the URL for the namespace page (with embed)', () => {
    fakeEmbed();
    const url = deepLinkURLFromScript('px/namespace', 'gke:foobar', withEmbed, {
      ...startTimeArgs,
      namespace: 'foobar',
    });
    expect(url).toEqual(
      '/live/clusters/gke%3Afoobar/namespaces/foobar?disable_time_picker=true&start_time=-7m');
  });

  it('should generate the URL for the nodes page', () => {
    const url = deepLinkURLFromScript('px/nodes', 'gke:foobar', noEmbed, startTimeArgs);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/nodes?start_time=-7m');
  });

  it('should generate the URL for the node page', () => {
    const url = deepLinkURLFromScript('px/node', 'gke:foobar', noEmbed, { node: 'node-123' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/nodes/node-123');
  });

  it('should generate the URL for the pods page', () => {
    const url = deepLinkURLFromScript('px/pods', 'gke:foobar', noEmbed, { namespace: 'foobar' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/foobar/pods');
  });

  it('should generate the URL for the pod page', () => {
    const url = deepLinkURLFromScript('px/pod', 'gke:foobar', noEmbed, { pod: 'ns/pod-123' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/ns/pods/pod-123');
  });

  it('should generate the URL for the services page', () => {
    const url = deepLinkURLFromScript('px/services', 'gke:foobar', noEmbed, { namespace: 'foobar' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/foobar/services');
  });

  it('should generate the URL for the IP page', () => {
    const url = deepLinkURLFromScript('px/ip', 'gke:foobar', noEmbed, { ip: '127.0.0.1' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar?ip=127.0.0.1&script=px%2Fip');
  });

  it('should generate the URL for the service page', () => {
    const url = deepLinkURLFromScript('px/service', 'gke:foobar', noEmbed, { service: 'ns/svc-123' });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/ns/services/svc-123');
  });

  it('should generate the URL for the service page (with service array)', () => {
    const url = deepLinkURLFromScript('px/service', 'gke:foobar', noEmbed, {
      service: '["px-sock-shop/orders", "px-sock-shop/carts"]',
    });
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders');
  });

  it('should generate a non-vanity URL (with no arguments)', () => {
    const url = deepLinkURLFromScript('px/somescript', 'gke:foobar', noEmbed, null);
    expect(url).toEqual('/live/clusters/gke%3Afoobar?script=px%2Fsomescript');
  });
});

describe('deepLinkURLFromSemanticType test', () => {
  beforeEach(restoreEmbed);
  afterEach(restoreEmbed);

  it('should generate the URL for an IP', () => {
    const url = deepLinkURLFromSemanticType(SemanticType.ST_IP_ADDRESS, '127.0.0.1',
      'gke:foobar', noEmbed);
    expect(url).toEqual('/live/clusters/gke%3Afoobar?ip=127.0.0.1&script=px%2Fip');
  });

  it('should generate the URL for a namespace', () => {
    const url = deepLinkURLFromSemanticType(SemanticType.ST_NAMESPACE_NAME, 'px-sock-shop',
      'gke:foobar', noEmbed);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop');
  });

  it('should generate the URL for a node', () => {
    const url = deepLinkURLFromSemanticType(SemanticType.ST_NODE_NAME, 'node-123', 'gke:foobar',
      noEmbed, startTimeArgs);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/nodes/node-123?start_time=-7m');
  });

  it('should generate the URL for a pod', () => {
    const url = deepLinkURLFromSemanticType(SemanticType.ST_POD_NAME, 'px-sock-shop/orders-123',
      'gke:foobar', noEmbed, startTimeArgs);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/pods/orders-123?start_time=-7m');
  });

  it('should generate the URL for a service', () => {
    fakeEmbed();
    const url = deepLinkURLFromSemanticType(SemanticType.ST_SERVICE_NAME, 'px-sock-shop/orders',
      'gke:foobar', withEmbed, null);
    expect(url).toEqual(
      '/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders?disable_time_picker=true');
  });

  it('should generate the URL for a service array', () => {
    const url = deepLinkURLFromSemanticType(SemanticType.ST_SERVICE_NAME,
      '["px-sock-shop/orders", "px-sock-shop/carts"]', 'gke:foobar', noEmbed);
    expect(url).toEqual('/live/clusters/gke%3Afoobar/namespaces/px-sock-shop/services/orders');
  });
});
