package controller_test

import (
	"testing"
	"time"

	"github.com/graph-gophers/graphql-go"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

func TestUserInfoResolver(t *testing.T) {
	sCtx := authcontext.New()
	sCtx.Claims = utils.GenerateJWTForUser("abcdef", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now())

	resolver := controller.UserInfoResolver{SessionCtx: sCtx}
	assert.Equal(t, "test@test.com", resolver.Email())
	assert.Equal(t, graphql.ID("abcdef"), resolver.ID())
}
