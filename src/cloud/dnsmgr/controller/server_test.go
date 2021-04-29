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

package controller_test

import (
	"context"
	"fmt"
	"os"
	"strings"
	"testing"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/dnsmgr/controller"
	mock_controller "px.dev/pixie/src/cloud/dnsmgr/controller/mock"
	"px.dev/pixie/src/cloud/dnsmgr/dnsmgrpb"
	"px.dev/pixie/src/cloud/dnsmgr/schema"
	"px.dev/pixie/src/shared/services/pgtest"
	"px.dev/pixie/src/utils"
)

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

var db *sqlx.DB

func testMain(m *testing.M) error {
	s := bindata.Resource(schema.AssetNames(), schema.Asset)
	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

func mustLoadTestData(db *sqlx.DB) {
	db.MustExec(`DELETE FROM dns_addresses`)
	db.MustExec(`DELETE FROM ssl_certs`)

	insertSSLCertsQuery := `INSERT INTO ssl_certs(cname, cluster_id, cert, key) VALUES ($1, $2, $3, $4)`
	db.MustExec(insertSSLCertsQuery, "abcd", "123e4567-e89b-12d3-a456-426655440000", "cert-abcd", "key-abcd")
	db.MustExec(insertSSLCertsQuery, "befgh", nil, "cert-befgh", "key-befgh")
	db.MustExec(insertSSLCertsQuery, "ijkl", nil, "cert-ijkl", "key-ijkl")
	db.MustExec(insertSSLCertsQuery, "default", nil, "cert-default", "key-default")

	insertDNSAddressQuery := `INSERT INTO dns_addresses(cluster_id, time_created, address) VALUES($1, $2, $3)`
	db.MustExec(insertDNSAddressQuery, "123e4567-e89b-12d3-a456-426655440000", "2011-05-16 15:36:38", "127.0.0.1")
}

func TestServer_GetDNSAddressExisting(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	req := &dnsmgrpb.GetDNSAddressRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
		IPAddress: "127.0.0.1",
	}

	s := controller.NewServer(nil, mockDNS, db)

	resp, err := s.GetDNSAddress(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "1305560198000000000.abcd.clusters.withpixie.ai", resp.DNSAddress)
}

func TestServer_GetDNSAddressDefault(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", true)
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetDNSAddressRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		IPAddress: "127.0.0.2",
	}

	var actualCname string
	mockDNS.EXPECT().
		CreateResourceRecord(gomock.Any(), gomock.Any(), gomock.Any()).
		DoAndReturn(func(cname string, addr string, ttl int64) error {
			actualCname = cname
			assert.True(t, strings.Contains(cname, ".default.clusters.withpixie.ai."))
			assert.NotEqual(t, "1305560198000000000.default.clusters.withpixie.ai", cname)
			assert.Equal(t, "127.0.0.2", addr)
			return nil
		})

	resp, err := s.GetDNSAddress(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, actualCname, resp.DNSAddress+".")
}

func TestServer_GetDNSAddressNew(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", false)

	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetDNSAddressRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
		IPAddress: "127.0.0.2",
	}

	var actualCname string
	mockDNS.EXPECT().
		CreateResourceRecord(gomock.Any(), gomock.Any(), gomock.Any()).
		DoAndReturn(func(cname string, addr string, ttl int64) error {
			actualCname = cname
			assert.True(t, strings.Contains(cname, ".abcd.clusters.withpixie.ai."))
			assert.NotEqual(t, "1305560198000000000.abcd.clusters.withpixie.ai", cname)
			assert.Equal(t, "127.0.0.2", addr)
			return nil
		})

	resp, err := s.GetDNSAddress(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, actualCname, resp.DNSAddress+".")
}

func TestServer_CreateSSLCertExisting(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-abcd", resp.Cert)
	assert.Equal(t, "key-abcd", resp.Key)
}

func TestServer_CreateNewSSLCert(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-befgh", resp.Cert)
	assert.Equal(t, "key-befgh", resp.Key)
}

func TestServer_CreateNewSSLCertDefault(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", true)
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-default", resp.Cert)
	assert.Equal(t, "key-default", resp.Key)
}
