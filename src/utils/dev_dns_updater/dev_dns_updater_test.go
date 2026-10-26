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

package main

import (
	"errors"
	"net"
	"testing"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
)

func TestWatchForExternalIPUsesResolvedHostname(t *testing.T) {
	restoreDNSState := setTestDNSState()
	defer restoreDNSState()

	oldLookupIP := lookupIP
	lookupIP = func(host string) ([]net.IP, error) {
		if host != "example.com" {
			t.Fatalf("unexpected hostname: %s", host)
		}
		return []net.IP{net.ParseIP("203.0.113.10")}, nil
	}
	defer func() { lookupIP = oldLookupIP }()

	events := make(chan watch.Event, 1)
	out := make(chan svcInfo, 1)
	events <- serviceEvent("cloud-proxy-service", "", "example.com")
	close(events)

	if err := watchForExternalIP(events, out); err != nil {
		t.Fatalf("watchForExternalIP returned error: %v", err)
	}

	got := <-out
	if got.SvcName != "cloud-proxy-service" {
		t.Fatalf("unexpected service name: %s", got.SvcName)
	}
	if got.Addr != "203.0.113.10" {
		t.Fatalf("unexpected address: %s", got.Addr)
	}
}

func TestWatchForExternalIPSkipsUnresolvedHostname(t *testing.T) {
	restoreDNSState := setTestDNSState()
	defer restoreDNSState()

	oldLookupIP := lookupIP
	lookupIP = func(host string) ([]net.IP, error) {
		return nil, errors.New("lookup failed")
	}
	defer func() { lookupIP = oldLookupIP }()

	events := make(chan watch.Event, 1)
	out := make(chan svcInfo, 1)
	events <- serviceEvent("cloud-proxy-service", "", "example.com")
	close(events)

	if err := watchForExternalIP(events, out); err != nil {
		t.Fatalf("watchForExternalIP returned error: %v", err)
	}

	if len(out) != 0 {
		t.Fatalf("expected no service updates, got %d", len(out))
	}
}

func setTestDNSState() func() {
	oldDNSEntries := dnsEntriesByService
	dnsEntriesByService = map[string][]string{
		"cloud-proxy-service": {"dev.withpixie.dev"},
	}
	return func() {
		dnsEntriesByService = oldDNSEntries
	}
}

func serviceEvent(name string, ip string, hostname string) watch.Event {
	return watch.Event{
		Object: &v1.Service{
			ObjectMeta: metav1.ObjectMeta{
				Name: name,
			},
			Status: v1.ServiceStatus{
				LoadBalancer: v1.LoadBalancerStatus{
					Ingress: []v1.LoadBalancerIngress{
						{
							IP:       ip,
							Hostname: hostname,
						},
					},
				},
			},
		},
	}
}
