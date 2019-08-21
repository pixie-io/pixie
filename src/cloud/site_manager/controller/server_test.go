package controllers_test

import (
	"context"
	"errors"
	"testing"

	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	controllers "pixielabs.ai/pixielabs/src/cloud/site_manager/controller"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/datastore"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/utils"
)

type fakeDatastore struct {
}

func (d *fakeDatastore) RegisterSite(orgID uuid.UUID, domainName string) error {
	return nil
}

func (d *fakeDatastore) CheckAvailability(domainName string) (bool, error) {
	if domainName == "is_available" {
		return true, nil
	}

	if domainName == "should_error" {
		return false, errors.New("something bad happened")
	}

	return false, nil
}

func (d *fakeDatastore) GetSiteForOrg(org uuid.UUID) (*datastore.SiteInfo, error) {
	if org.String() == "123e4567-e89b-12d3-a456-426655440000" {
		return &datastore.SiteInfo{
			OrgID:      uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			DomainName: "hulu",
		}, nil
	}
	if org.String() == "323e4567-e89b-12d3-a456-426655440000" {
		return nil, errors.New("internal error")
	}
	return nil, nil
}

func (d *fakeDatastore) GetSiteByDomain(domainName string) (*datastore.SiteInfo, error) {
	if domainName == "hulu" {
		return &datastore.SiteInfo{
			OrgID:      uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			DomainName: "hulu",
		}, nil
	}
	if domainName == "goo" {
		return nil, errors.New("badness")
	}
	// Not found.
	return nil, nil
}

func SetupServerTest(t *testing.T) *controllers.Server {
	server := controllers.NewServer(&fakeDatastore{})
	assert.NotNil(t, server)
	return server
}

func TestServer_IsSiteAvailable(t *testing.T) {
	server := SetupServerTest(t)

	req := &sitemanagerpb.IsSiteAvailableRequest{
		DomainName: "is_available",
	}
	resp, err := server.IsSiteAvailable(context.Background(), req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	assert.True(t, resp.Available)
}

func TestServer_IsSiteAvailableNotAvailable(t *testing.T) {
	server := SetupServerTest(t)

	req := &sitemanagerpb.IsSiteAvailableRequest{
		DomainName: "asdsad",
	}
	resp, err := server.IsSiteAvailable(context.Background(), req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	assert.False(t, resp.Available)
}

func TestServer_IsSiteAvailableError(t *testing.T) {
	server := SetupServerTest(t)

	req := &sitemanagerpb.IsSiteAvailableRequest{
		DomainName: "should_error",
	}
	resp, err := server.IsSiteAvailable(context.Background(), req)

	assert.NotNil(t, err)
	assert.Nil(t, resp)

	assert.Equal(t, err.Error(), "something bad happened")
}

func TestServer_GetSiteByDomain(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("domain found", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByDomainRequest{DomainName: "hulu"}
		resp, err := server.GetSiteByDomain(context.Background(), req)
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.DomainName, "hulu")
		assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
	})

	t.Run("domain not found", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByDomainRequest{DomainName: "somedomain"}
		resp, err := server.GetSiteByDomain(context.Background(), req)
		assert.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.NotFound, s.Code())
	})

	t.Run("failed datastore request", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByDomainRequest{DomainName: "goo"}
		resp, err := server.GetSiteByDomain(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.Internal, s.Code())
	})

	t.Run("request with empty domain", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByDomainRequest{}
		resp, err := server.GetSiteByDomain(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.InvalidArgument, s.Code())
	})
}

func TestServer_GetSiteForOrg(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("domain found", func(t *testing.T) {
		resp, err := server.GetSiteForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.DomainName, "hulu")
		assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
	})

	t.Run("not found", func(t *testing.T) {
		resp, err := server.GetSiteForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		require.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.NotFound, s.Code())
	})

	t.Run("error with backend", func(t *testing.T) {
		resp, err := server.GetSiteForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		require.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.Internal, s.Code())
	})
}
