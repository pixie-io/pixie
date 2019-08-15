package controllers_test

import (
	"context"
	"errors"
	"testing"

	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	controllers "pixielabs.ai/pixielabs/src/services/site_manager/controller"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerpb"
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

func SetupServerTest(t *testing.T) *controllers.Server {
	server, err := controllers.NewServer(nil, &fakeDatastore{})
	assert.Nil(t, err)
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
