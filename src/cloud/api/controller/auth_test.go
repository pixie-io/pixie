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

	"github.com/dgrijalva/jwt-go/v4"
	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestGetServiceCredentials(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	tokenString, err := controller.GetServiceCredentials("jwt-key")
	require.NoError(t, err)
	token, err := jwt.ParseWithClaims(tokenString, &jwt.MapClaims{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("Unexpected signing method: %v", token.Header["alg"])
		}
		return []byte("jwt-key"), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	claims := token.Claims.(*jwt.MapClaims)
	assert.Nil(t, claims.Valid(jwt.NewValidationHelper(jwt.WithAudience("withpixie.ai"))))
}

func TestAuthSignupHandler(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/signup",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.SignupRequest{
		AccessToken: "the-token",
	}

	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	signupReply := &authpb.SignupReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgID: pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}
	mockClients.MockAuth.EXPECT().Signup(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.SignupRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(signupReply, nil)

	getOrgReply := &profilepb.OrgInfo{
		OrgName: "defg.com",
	}
	mockClients.MockProfile.EXPECT().GetOrg(gomock.Any(), gomock.Any()).Do(func(ctx context.Context, in *uuidpb.UUID) {
		assert.Equal(t, pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"), in)
	}).Return(getOrgReply, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthSignupHandler)
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
		OrgInfo struct {
			OrgID   string `json:"orgID"`
			OrgName string `json:"orgName"`
		} `json:"orgInfo"`
		OrgCreated bool `json:"orgCreated"`
	}
	err = json.NewDecoder(rr.Body).Decode(&parsedResponse)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, parsedResponse.Token)
	assert.Equal(t, testTokenExpiry, parsedResponse.ExpiresAt)
	assert.Equal(t, "abc@defg.com", parsedResponse.UserInfo.Email)
	assert.Equal(t, "first", parsedResponse.UserInfo.FirstName)
	assert.Equal(t, "last", parsedResponse.UserInfo.LastName)

	// Check the token in the cookie.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthLoginHandler(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: true,
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.LoginReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}
	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
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
		OrgInfo     struct {
			OrgID   string `json:"orgID"`
			OrgName string `json:"orgName"`
		} `json:"orgInfo"`
	}
	err = json.NewDecoder(rr.Body).Decode(&parsedResponse)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, parsedResponse.Token)
	assert.Equal(t, testTokenExpiry, parsedResponse.ExpiresAt)
	assert.Equal(t, "abc@defg.com", parsedResponse.UserInfo.Email)
	assert.Equal(t, "first", parsedResponse.UserInfo.FirstName)
	assert.Equal(t, "last", parsedResponse.UserInfo.LastName)
	assert.Equal(t, false, parsedResponse.UserCreated)
	assert.Equal(t, "test", parsedResponse.OrgInfo.OrgID)
	assert.Equal(t, "testOrg", parsedResponse.OrgInfo.OrgName)

	// Check the token in the cookie.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthLoginHandler_WithOrgName(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\", \"orgName\": \"hulu\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: true,
		OrgName:               "hulu",
	}

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
}

func TestAuthLoginHandler_FailedAuthServiceRequestFailed(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: true,
	}

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, status.New(codes.Unauthenticated, "bad token").Err())

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestAuthLoginHandler_FailedAuthRequest(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: true,
	}

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Do(func(ctx context.Context, in *authpb.LoginRequest) {
		assert.Equal(t, "the-token", in.AccessToken)
	}).Return(nil, errors.New("badness"))

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)
	fmt.Print(rr.Body.String())
	assert.Equal(t, http.StatusInternalServerError, rr.Code)
}

func TestAuthLoginHandler_BadMethod(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("GET", "/login", nil)
	require.NoError(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLoginHandler)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}

func TestAuthLogoutHandler(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/logout", nil)
	require.NoError(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)

	rawCookies := rr.Header().Get("Set-Cookie")

	// The cookies are encrypted so it's hard to validate the values of the actual cookie.
	// This is a *not* great test to make sure certain properties are set, so the browser correctly marks
	// the cookie as dead.
	assert.Contains(t, rawCookies, "Domain=withpixie.ai")
	assert.Contains(t, rawCookies, "Path=/")
	assert.Contains(t, rawCookies, "Max-Age=0")
	assert.Contains(t, rawCookies, "HttpOnly")
	assert.Contains(t, rawCookies, "Secure")

	// Check the cookie is removed from session.
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controller.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, "", sess.Values["_at"])
}

func TestAuthLogoutHandler_BadMethod(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "/logout", nil)
	require.NoError(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controller.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}
