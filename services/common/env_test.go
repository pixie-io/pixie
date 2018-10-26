package common_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/utils/testingutils"
)

func TestEnv_ParseJWTClaims(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "abc")

	benv := common.Env{
		ExternalAddress: "https://testing.com",
		SigningKey:      "abc",
	}

	claims, err := benv.ParseJWTClaims(token)
	assert.Nil(t, err)
	assert.Equal(t, "test", claims.UserID)
	assert.Equal(t, "test@test.com", claims.Email)
}

func TestEnv_ParseJWTClaims_BadKey(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "abc")

	benv := common.Env{
		ExternalAddress: "https://testing.com",
		SigningKey:      "badness",
	}

	claims, err := benv.ParseJWTClaims(token)
	assert.NotNil(t, err)
	assert.Nil(t, claims)
}
