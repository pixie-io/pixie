package authcontext_test

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	pb "pixielabs.ai/pixielabs/src/shared/services/proto"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestSessionCtx_UseJWTAuth(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token, "withpixie.ai")
	require.NoError(t, err)

	assert.Equal(t, testingutils.TestUserID, ctx.Claims.Subject)
	assert.Equal(t, "test@test.com", ctx.Claims.GetUserClaims().Email)
}

func TestSessionCtx_ValidClaims(t *testing.T) {
	tests := []struct {
		name          string
		expiryFromNow time.Duration
		claims        *pb.JWTClaims
		isValid       bool
	}{
		{
			name:    "no claims",
			isValid: false,
		},
		{
			name:          "valid user claims",
			isValid:       true,
			claims:        testingutils.GenerateTestClaimsWithDuration(t, time.Minute*60, "test@test.com"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired user claims",
			isValid:       false,
			claims:        testingutils.GenerateTestClaimsWithDuration(t, time.Minute*60, "test@test.com"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:          "valid service claims",
			isValid:       true,
			claims:        testingutils.GenerateTestServiceClaims(t, "vzmgr"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired service claims",
			isValid:       false,
			claims:        testingutils.GenerateTestServiceClaims(t, "vzmgr"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:          "valid cluster claims",
			isValid:       true,
			claims:        utils.GenerateJWTForCluster("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "withpixie.ai"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "invalid cluster ID",
			isValid:       false,
			claims:        utils.GenerateJWTForCluster("some text", "withpixie.ai"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired cluster claims",
			isValid:       false,
			claims:        utils.GenerateJWTForCluster("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "withpixie.ai"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:    "claims with no type",
			isValid: false,
			claims: &pb.JWTClaims{
				Subject:   "test subject",
				Audience:  "withpixie.ai",
				IssuedAt:  time.Now().Unix(),
				ExpiresAt: time.Now().Add(time.Minute * 10).Unix(),
			},
			expiryFromNow: time.Minute * 60,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			ctx := authcontext.New()
			if tc.claims != nil {
				token := testingutils.SignPBClaims(t, tc.claims, "signing_key")
				err := ctx.UseJWTAuth("signing_key", token, "withpixie.ai")
				require.NoError(t, err)

				ctx.Claims.ExpiresAt = time.Now().Add(tc.expiryFromNow).Unix()
			}

			assert.Equal(t, tc.isValid, ctx.ValidClaims())
		})
	}
}
