package authcontext_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestNew(t *testing.T) {
	ctx := authcontext.New()

	assert.Nil(t, ctx.Claims)
	assert.False(t, ctx.ValidClaims())
}

func TestSessionCtx_UseJWTAuth(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.Equal(t, testingutils.TestUserID, ctx.Claims.Subject)
	assert.Equal(t, "test@test.com", ctx.Claims.GetUserClaims().Email)
}

func TestSessionCtx_ValidClaims(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.True(t, ctx.ValidClaims())
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
