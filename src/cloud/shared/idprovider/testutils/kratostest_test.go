package testutils_test

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	"px.dev/pixie/src/cloud/shared/idprovider/testutils"
)

func TestCreateKratos(t *testing.T) {
	server, err := testutils.NewKratosServer()

	if err != nil {
		t.Fatal("failed to create server")
	}

	defer server.CleanUp()

	client, closeServer, err := server.Serve()
	defer closeServer()
	if err != nil {
		t.Fatal("failed to start server")
	}

	// Simple test that should pass if the client is setup properly.
	email := "bobloblaw@lawblog.com"
	idResp, err := client.CreateInviteLink(
		context.Background(),
		&idmanager.CreateInviteLinkRequest{
			Email:    email,
			PLUserID: email,
			PLOrgID:  "lawblow.com",
		},
	)
	if err != nil {
		t.Fatal("failed to call client")
	}
	assert.Equal(t, idResp.Email, email)
}
