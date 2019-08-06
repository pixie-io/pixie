package controller_test

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/services/api/controller"
	"pixielabs.ai/pixielabs/src/services/api/controller/testutils"
	authpb "pixielabs.ai/pixielabs/src/services/auth/proto"
	"pixielabs.ai/pixielabs/src/services/common/handler"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestGetServiceCredentials(t *testing.T) {
	tokenString, err := controller.GetServiceCredentials("jwt-key")
	assert.Nil(t, err)
	token, err := jwt.ParseWithClaims(tokenString, &jwt.MapClaims{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("Unexpected signing method: %v", token.Header["alg"])
		}
		return []byte("jwt-key"), nil
	})
	assert.Nil(t, err)
	claims := token.Claims.(*jwt.MapClaims)
	assert.Nil(t, claims.Valid())
}

func TestAuthLoginHandler(t *testing.T) {
	env, mockAuthClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken: "the-token",
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.LoginReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
	}
	mockAuthClient.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusOK, rr.Code)
	var parsedResponse struct {
		Token     string
		ExpiresAt int64
	}
	err = json.NewDecoder(rr.Body).Decode(&parsedResponse)
	assert.Nil(t, err)
	assert.Equal(t, testReplyToken, parsedResponse.Token)
	assert.Equal(t, testTokenExpiry, parsedResponse.ExpiresAt)

	// Check the token in the cookie.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthLoginHandler_FailedAuthServiceRequestFailed(t *testing.T) {
	env, mockAuthClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken: "the-token",
	}

	mockAuthClient.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, status.New(codes.Unauthenticated, "bad token").Err())

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
	fmt.Printf(rr.Body.String())
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestAuthLoginHandler_FailedAuthRequest(t *testing.T) {
	env, mockAuthClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken: "the-token",
	}

	mockAuthClient.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, errors.New("badness"))

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
	fmt.Printf(rr.Body.String())
	assert.Equal(t, http.StatusInternalServerError, rr.Code)
}

func TestAuthLoginHandler_BadMethod(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}

func TestAuthLogoutHandler(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)

	// Check the cookie is removed from session.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	assert.Equal(t, "", sess.Values["_at"])
}

func TestAuthLogoutHandler_BadMethod(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}
