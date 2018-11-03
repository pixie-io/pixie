package controller_test

import (
	"testing"

	"github.com/graph-gophers/graphql-go"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/services/api/controller"
	pb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
)

func TestUserInfoResolver(t *testing.T) {
	sCtx := sessioncontext.New()
	sCtx.Claims = &pb.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"

	resolver := controller.UserInfoResolver{sCtx}
	assert.Equal(t, "test@test.com", resolver.Email())
	assert.Equal(t, graphql.ID("abcdef"), resolver.ID())
}
