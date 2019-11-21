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
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	pbutils "pixielabs.ai/pixielabs/src/utils"
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
	env, mockAuthClient, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\", \"siteName\": \"hulu\", \"userEmail\": \"user@hulu.com\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		SiteName:              "hulu",
		CreateUserIfNotExists: true,
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.LoginReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
		UserInfo: &authpb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
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
		UserInfo  struct {
			UserID    string `json:"userID"`
			FirstName string `json:"firstName"`
			LastName  string `json:"lastName"`
			Email     string `json:"email"`
		} `json:"userInfo"`
		UserCreated bool `json:"userCreated"`
	}
	err = json.NewDecoder(rr.Body).Decode(&parsedResponse)
	assert.Nil(t, err)
	assert.Equal(t, testReplyToken, parsedResponse.Token)
	assert.Equal(t, testTokenExpiry, parsedResponse.ExpiresAt)
	assert.Equal(t, "abc@defg.com", parsedResponse.UserInfo.Email)
	assert.Equal(t, "first", parsedResponse.UserInfo.FirstName)
	assert.Equal(t, "last", parsedResponse.UserInfo.LastName)
	assert.Equal(t, false, parsedResponse.UserCreated)

	// Check the token in the cookie.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
	assert.Equal(t, "hulu", sess.Values["_auth_site"])
}

func TestAuthLoginHandler_ExistingSessionMismatchedSite(t *testing.T) {
	env, mockAuthClient, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\", \"siteName\": \"hulu\", \"userEmail\": \"user@hulu.com\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		SiteName:              "hulu",
		CreateUserIfNotExists: true,
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.LoginReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
		UserInfo: &authpb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
		},
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

	// Check the token in the cookie for this first pass
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
	assert.Equal(t, "hulu", sess.Values["_auth_site"])

	// Now let's try the same thing but a different site
	expectedAuthServiceReq = &authpb.LoginRequest{
		AccessToken:           "the-token-2",
		SiteName:              "not_hulu",
		CreateUserIfNotExists: true,
	}
	testReplyToken2 := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry2 := time.Now().Add(1 * time.Minute).Unix()
	loginResp = &authpb.LoginReply{
		Token:     testReplyToken2,
		ExpiresAt: testTokenExpiry2,
		UserInfo: &authpb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
		},
	}
	mockAuthClient.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token-2", in.AccessToken)
	}).Return(loginResp, nil)

	req3, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token-2\", \"siteName\": \"not_hulu\", \"userEmail\": \"user@not_hulu.com\"}"))
	assert.Nil(t, err)

	rr2 := httptest.NewRecorder()
	h2 := handler.New(env, controller.AuthLoginHandler)

	h2.ServeHTTP(rr2, req3)

	assert.Equal(t, http.StatusOK, rr2.Code)
	var parsedResponse2 struct {
		Token     string
		ExpiresAt int64
	}
	err = json.NewDecoder(rr2.Body).Decode(&parsedResponse2)
	assert.Nil(t, err)
	assert.Equal(t, testReplyToken2, parsedResponse2.Token)
	assert.Equal(t, testTokenExpiry2, parsedResponse2.ExpiresAt)

	rawCookies2 := rr2.Header().Get("Set-Cookie")
	header2 := http.Header{}
	header2.Add("Cookie", rawCookies2)
	req4 := http.Request{Header: header2}
	sess2, err := controller.GetDefaultSession(env, &req4)
	assert.Equal(t, testReplyToken2, sess2.Values["_at"])
	assert.Equal(t, "not_hulu", sess2.Values["_auth_site"])
}

func TestAuthLoginHandler_FailedAuthServiceRequestFailed(t *testing.T) {
	env, mockAuthClient, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\", \"siteName\": \"hulu\", \"userEmail\": \"user@gmail.com\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		SiteName:              "hulu",
		CreateUserIfNotExists: true,
	}

	mockAuthClient.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, status.New(codes.Unauthenticated, "bad token").Err())

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestAuthLoginHandler_FailedAuthRequest(t *testing.T) {
	env, mockAuthClient, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/api/users",
		strings.NewReader("{\"accessToken\": \"the-token\", \"siteName\": \"hulu\", \"userEmail\": \"user@hulu.com\"}"))
	assert.Nil(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		SiteName:              "hulu",
		CreateUserIfNotExists: true,
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
	env, _, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}

func TestAuthLogoutHandler(t *testing.T) {
	env, _, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/logout", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)

	rawCookies := rr.Header().Get("Set-Cookie")

	// The cookies are encrypted so it's hard to validate the values of the actual cookie.
	// This is a *not* great test to make sure certain properties are set, so the browser correctly marks
	// the cookie as dead.
	assert.Contains(t, rawCookies, "Domain=example.com")
	assert.Contains(t, rawCookies, "Path=/")
	assert.Contains(t, rawCookies, "Max-Age=0")
	assert.Contains(t, rawCookies, "HttpOnly")
	assert.Contains(t, rawCookies, "Secure")

	// Check the cookie is removed from session.
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	assert.Equal(t, "", sess.Values["_at"])
}

func TestAuthLogoutHandler_BadMethod(t *testing.T) {
	env, _, _, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}
