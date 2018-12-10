package sessioncontext_test

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestNew(t *testing.T) {
	ctx := sessioncontext.New()

	assert.Nil(t, ctx.Claims)
	assert.False(t, ctx.ValidUser())
}

func TestSessionCtx_UseJWTAuth(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := sessioncontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.Equal(t, "test", ctx.Claims.Subject)
	assert.Equal(t, "test@test.com", ctx.Claims.Email)
}

func TestSessionCtx_ValidUser(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := sessioncontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	assert.True(t, ctx.ValidUser())
}

func TestSessionCtx_ValidUser_Expired(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := sessioncontext.New()
	err := ctx.UseJWTAuth("signing_key", token)
	assert.Nil(t, err)

	ctx.Claims.ExpiresAt = time.Now().Add(-1 * time.Second).Unix()
	assert.False(t, ctx.ValidUser())
}
