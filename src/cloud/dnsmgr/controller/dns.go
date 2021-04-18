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

package controller

import (
	"context"

	"google.golang.org/api/dns/v1"
	"google.golang.org/api/option"
)

// DNSService is a service that can get and update DNS records.
type DNSService interface {
	CreateResourceRecord(name string, data string, ttl int64) error
}

// DNSRecord represents a DNS record.
type DNSRecord struct {
	Name string
	Data []string
}

// CloudDNSService is the Cloud DNS service.
type CloudDNSService struct {
	DNSZone     string
	DNSProject  string
	SvcAcctFile string
	dnsService  *dns.Service
}

// NewCloudDNSService creates a new Cloud DNS service.
func NewCloudDNSService(dnsZone string, dnsProject string, svcAcctFile string) (*CloudDNSService, error) {
	ctx := context.Background()

	dnsService, err := dns.NewService(ctx, option.WithCredentialsFile(svcAcctFile))
	if err != nil {
		return nil, err
	}

	return &CloudDNSService{dnsZone, dnsProject, svcAcctFile, dnsService}, nil
}

// CreateResourceRecord creates the resource record with the given name and data.
func (s *CloudDNSService) CreateResourceRecord(name string, data string, ttl int64) error {
	cSvc := dns.NewChangesService(s.dnsService)

	resourceRecord := &dns.ResourceRecordSet{
		Name:    name,
		Rrdatas: []string{data},
		Type:    "A",
		Ttl:     ttl,
	}

	change := &dns.Change{
		Additions: []*dns.ResourceRecordSet{resourceRecord},
	}

	_, err := cSvc.Create(s.DNSProject, s.DNSZone, change).Do()
	return err
}
