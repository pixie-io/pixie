package authcontext_test

import (
	"testing"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestNew(t *testing.T) {
	ctx := authcontext.New()

	assert.Nil(t, ctx.Claims)
	assert.False(t, ctx.ValidUser())
}

func TestSessionCtx_UseJWTAuth(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.Equal(t, testingutils.TestUserID, ctx.Claims.Subject)
	assert.Equal(t, "test@test.com", ctx.Claims.GetUserClaims().Email)
}

func TestSessionCtx_ValidUser(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.True(t, ctx.ValidUser())
}

// TODO(michelle): Delete this or update this when scoped tokens are implemented.
//func TestSessionCtx_ValidUser_Expired(t *testing.T) {
//	token := testingutils.GenerateTestJWTToken(t, "signing_key")
//
//	ctx := authcontext.New()
//	err := ctx.UseJWTAuth("signing_key", token)
//	assert.Nil(t, err)
//
//	ctx.Claims.ExpiresAt = time.Now().Add(-1 * time.Second).Unix()
//	assert.False(t, ctx.ValidUser())
//}
