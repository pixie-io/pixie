package controller_test

import (
	"testing"

	"github.com/graph-gophers/graphql-go"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/services/api/controller"
	"pixielabs.ai/pixielabs/src/services/common/authcontext"
	pb "pixielabs.ai/pixielabs/src/services/common/proto"
)

func TestUserInfoResolver(t *testing.T) {
	sCtx := authcontext.New()
	sCtx.Claims = &pb.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"

	resolver := controller.UserInfoResolver{SessionCtx: sCtx}
	assert.Equal(t, "test@test.com", resolver.Email())
	assert.Equal(t, graphql.ID("abcdef"), resolver.ID())
}
