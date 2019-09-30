package controller_test

import (
	"context"
	"strings"
	"testing"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller"
	mock_controller "pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller/mock"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/schema"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
	"pixielabs.ai/pixielabs/src/utils"
)

func setupTestDB(t *testing.T) (*sqlx.DB, func()) {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})
	db, teardown := pgtest.SetupTestDB(t, s)

	return db, func() {
		teardown()
	}
}

func loadTestData(t *testing.T, db *sqlx.DB) {
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
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	req := &dnsmgrpb.GetDNSAddressRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
		IPAddress: "127.0.0.1",
	}

	s := controller.NewServer(nil, mockDNS, db)

	mockDNS.EXPECT().
		CreateResourceRecord("1305560198000000000.abcd.clusters.withpixie.ai", "127.0.0.1", int64(controller.ResourceRecordTTL)).
		Return(nil)

	resp, err := s.GetDNSAddress(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "1305560198000000000.abcd.clusters.withpixie.ai", resp.DNSAddress)
}

func TestServer_GetDNSAddressDefault(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", true)
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
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
			assert.True(t, strings.Contains(cname, ".default.clusters.withpixie.ai"))
			assert.NotEqual(t, "1305560198000000000.default.clusters.withpixie.ai", cname)
			assert.Equal(t, "127.0.0.2", addr)
			return nil
		})

	resp, err := s.GetDNSAddress(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, actualCname, resp.DNSAddress)
}

func TestServer_GetDNSAddressNew(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", false)

	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
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
			assert.True(t, strings.Contains(cname, ".abcd.clusters.withpixie.ai"))
			assert.NotEqual(t, "1305560198000000000.abcd.clusters.withpixie.ai", cname)
			assert.Equal(t, "127.0.0.2", addr)
			return nil
		})

	resp, err := s.GetDNSAddress(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, actualCname, resp.DNSAddress)
}

func TestServer_CreateSSLCertExisting(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-abcd", resp.Cert)
	assert.Equal(t, "key-abcd", resp.Key)
}

func TestServer_CreateNewSSLCert(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-befgh", resp.Cert)
	assert.Equal(t, "key-befgh", resp.Key)
}

func TestServer_CreateNewSSLCertDefault(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("use_default_dns_cert", true)
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS, db)

	req := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}

	resp, err := s.GetSSLCerts(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "cert-default", resp.Cert)
	assert.Equal(t, "key-default", resp.Key)
}
