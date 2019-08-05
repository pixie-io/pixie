package controller_test

import (
	"testing"

	"github.com/graph-gophers/graphql-go"
	"github.com/stretchr/testify/assert"
	pb "pixielabs.ai/pixielabs/src/services/common/proto"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller"
)

func TestUserInfoResolver(t *testing.T) {
	sCtx := sessioncontext.New()
	sCtx.Claims = &pb.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"

	resolver := controller.UserInfoResolver{SessionCtx: sCtx}
	assert.Equal(t, "test@test.com", resolver.Email())
	assert.Equal(t, graphql.ID("abcdef"), resolver.ID())
}
