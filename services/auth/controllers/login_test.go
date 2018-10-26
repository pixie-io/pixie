package controllers_test

import (
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"golang.org/x/net/context"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/services/auth/controllers"
	"pixielabs.ai/pixielabs/services/auth/controllers/mock"
	pb "pixielabs.ai/pixielabs/services/auth/proto"
	"pixielabs.ai/pixielabs/services/common"
	jwtpb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/utils/testingutils"
)

func getTestContext() context.Context {
	env := controllers.AuthEnv{
		Env: &common.Env{
			SigningKey: "jwtkey",
			Claims: &jwtpb.JWTClaims{
				Email: "test@test.com",
			},
		},
	}
	return context.WithValue(context.Background(), common.EnvKey, &env)
}

func TestServer_Login(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{},
	}
	a.EXPECT().SetPLUserID("userid", gomock.Any()).Do(func(uid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata.PLUserID = plid

	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	s, err := controllers.NewServer(a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata.PLUserID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Login_BadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("", errors.New("bad token"))

	s, err := controllers.NewServer(a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.NotNil(t, err)
	// Check the response data.
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.Unauthenticated, stat.Code())
	assert.Nil(t, resp)
}

func TestServer_Login_HasPLUserID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo1 := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{
			PLUserID: "pluserid",
		},
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo1, nil)

	s, err := controllers.NewServer(a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, "pluserid", resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	s, err := controllers.NewServer(a)
	assert.Nil(t, err)

	token := testingutils.GenerateTestJWTToken(t, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	resp, err := s.GetAugmentedToken(getTestContext(), req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	assert.Equal(t, token, resp.Token)
	fmt.Printf("resp: %v", resp)
	assert.Equal(t, "test@test.com", resp.Claims.Email)
}

func verifyToken(t *testing.T, token, expectedUserID string, expectedExpiry int64, key string) {
	claims := jwt.MapClaims{}
	_, err := jwt.ParseWithClaims(token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(key), nil
	})
	assert.Nil(t, err)
	assert.Equal(t, expectedUserID, claims["UserID"])
	assert.Equal(t, expectedExpiry, int64(claims["exp"].(float64)))
}

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*pb.LoginReply, error) {
	req := &pb.LoginRequest{
		AccessToken: "tokenabc",
	}
	return server.Login(ctx, req)
}
