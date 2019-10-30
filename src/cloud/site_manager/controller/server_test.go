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

func (d *fakeDatastore) RegisterSite(orgID uuid.UUID, siteName string) error {
	return nil
}

func (d *fakeDatastore) CheckAvailability(siteName string) (bool, error) {
	if siteName == "is-available" {
		return true, nil
	}

	if siteName == "should-error" {
		return false, errors.New("something bad happened")
	}

	return false, nil
}

func (d *fakeDatastore) GetSiteForOrg(org uuid.UUID) (*datastore.SiteInfo, error) {
	if org.String() == "123e4567-e89b-12d3-a456-426655440000" {
		return &datastore.SiteInfo{
			OrgID:    uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			SiteName: "hulu",
		}, nil
	}
	if org.String() == "323e4567-e89b-12d3-a456-426655440000" {
		return nil, errors.New("internal error")
	}
	return nil, nil
}

func (d *fakeDatastore) GetSiteByName(siteName string) (*datastore.SiteInfo, error) {
	if siteName == "hulu" {
		return &datastore.SiteInfo{
			OrgID:    uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			SiteName: "hulu",
		}, nil
	}
	if siteName == "goo" {
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

	tests := []struct {
		name        string
		siteName    string
		isAvailable bool
		expectError bool
	}{
		{
			name:        "Valid site should return available",
			siteName:    "is-available",
			isAvailable: true,
			expectError: false,
		},
		{
			name:        "Should ignore case on site check",
			siteName:    "iS-Available",
			isAvailable: true,
			expectError: false,
		},
		{
			name:        "Error on DB lookup should return error",
			siteName:    "should-error",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "Used site should return not-available",
			siteName:    "already-used",
			isAvailable: false,
			expectError: false,
		},
		{
			name:        "Blacklisted sites are not available",
			siteName:    "cloud",
			isAvailable: false,
			expectError: false,
		},
		{
			name:        "invalid domain name should be an error",
			siteName:    "-abc",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "invalid domain name should be an error",
			siteName:    "A--abc",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "invalid domain name should be an error",
			siteName:    "a_b",
			isAvailable: false,
			expectError: true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			req := &sitemanagerpb.IsSiteAvailableRequest{
				SiteName: test.siteName,
			}
			resp, err := server.IsSiteAvailable(context.Background(), req)
			if test.expectError {
				assert.NotNil(t, err)
			} else {
				require.Nil(t, err)
				assert.Equal(t, resp.Available, test.isAvailable)
			}
		})
	}
}

func TestServer_RegisterSiteBlacklist(t *testing.T) {
	server := SetupServerTest(t)

	req := &sitemanagerpb.RegisterSiteRequest{
		SiteName: "cloud",
	}
	resp, err := server.RegisterSite(context.Background(), req)

	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_GetSiteByName(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("site found", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByNameRequest{SiteName: "hulu"}
		resp, err := server.GetSiteByName(context.Background(), req)
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.SiteName, "hulu")
		assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
	})

	t.Run("site not found", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByNameRequest{SiteName: "somesite"}
		resp, err := server.GetSiteByName(context.Background(), req)
		assert.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.NotFound, s.Code())
	})

	t.Run("failed datastore request", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByNameRequest{SiteName: "goo"}
		resp, err := server.GetSiteByName(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.Internal, s.Code())
	})

	t.Run("request with empty site", func(t *testing.T) {
		req := &sitemanagerpb.GetSiteByNameRequest{}
		resp, err := server.GetSiteByName(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.InvalidArgument, s.Code())
	})
}

func TestServer_GetSiteForOrg(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("site found", func(t *testing.T) {
		resp, err := server.GetSiteForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.SiteName, "hulu")
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
