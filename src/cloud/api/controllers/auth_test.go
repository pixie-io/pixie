/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers_test

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/lestrrat-go/jwx/jwt"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/shared/services/handler"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestGetServiceCredentials(t *testing.T) {
	viper.Set("domain_name", "withpixie.ai")
	tokenString, err := controllers.GetServiceCredentials("jwt-key")
	require.NoError(t, err)

	token, err := srvutils.ParseToken(tokenString, "jwt-key", "withpixie.ai")
	require.NoError(t, err)
	err = jwt.Validate(token, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
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
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgID: utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}
	mockClients.MockAuth.EXPECT().Signup(gomock.Any(), expectedAuthServiceReq).Return(signupReply, nil)
	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthSignupHandler)
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
	sess, err := controllers.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthSignupHandler_IDToken(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/signup",
		strings.NewReader("{\"accessToken\": \"the-token\", \"idToken\": \"the-id-token\"}"))
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
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgID: utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}
	mockClients.MockAuth.EXPECT().Signup(gomock.Any(), expectedAuthServiceReq).Return(signupReply, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthSignupHandler)
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
	sess, err := controllers.GetDefaultSession(env, &req2)
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
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}
	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
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
	sess, err := controllers.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthLoginHandler_WithIDToken(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\", \"idToken\": \"the-id-token\"}"))
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
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}
	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
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
	sess, err := controllers.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}

func TestAuthLoginHandler_WithOrgName(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\", \"orgName\": \"my-org\"}"))
	require.NoError(t, err)

	// After support account removal, we ignore the orgName  parameter
	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: true,
	}

	loginResp := &authpb.LoginReply{
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
	h.ServeHTTP(rr, req)
}

func TestAuthLoginHandler_WithInviteToken(t *testing.T) {
	// Simple API forwarding check.
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"access-token\", \"inviteToken\": \"invite-token\"}"))
	require.NoError(t, err)

	// After support account removal, we ignore the orgName  parameter
	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "access-token",
		InviteToken:           "invite-token",
		CreateUserIfNotExists: true,
	}

	loginResp := &authpb.LoginReply{
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).
		Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
	h.ServeHTTP(rr, req)
}

func TestAuthSignupHandler_WithInviteToken(t *testing.T) {
	// Simple API forwarding check.
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	req, err := http.NewRequest("POST", "/signup",
		strings.NewReader("{\"accessToken\": \"access-token\", \"inviteToken\": \"invite-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.SignupRequest{
		AccessToken: "access-token",
		InviteToken: "invite-token",
	}

	signupResp := &authpb.SignupReply{
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgID: utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}

	mockClients.MockAuth.EXPECT().Signup(gomock.Any(), expectedAuthServiceReq).
		Return(signupResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthSignupHandler)
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

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).
		Return(nil, status.New(codes.Unauthenticated, "bad token").Err())

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
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

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).
		Return(nil, errors.New("badness"))

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
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
	h := handler.New(env, controllers.AuthLoginHandler)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}

func TestAuthLogoutHandler(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/logout", nil)
	require.NoError(t, err)

	// Setup the auth token, otherwise logout errors out.
	sess, err := controllers.GetDefaultSession(env, req)
	require.NoError(t, err)
	sess.Values["_at"] = testingutils.GenerateTestJWTToken(t, "jwt-key")
	cookieRecorder := httptest.NewRecorder()
	require.NoError(t, sess.Save(req, cookieRecorder))
	req.Header.Add("Cookie", cookieRecorder.Header().Get("Set-Cookie"))

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLogoutHandler)

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
	sess, err = controllers.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, "", sess.Values["_at"])
}

func TestAuthLogoutHandler_BadMethod(t *testing.T) {
	env, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "/logout", nil)
	require.NoError(t, err)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLogoutHandler)

	h.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusMethodNotAllowed, rr.Code)
}

func TestAuthLoginHandlerEmbed(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{\"accessToken\": \"the-token\"}"))
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.LoginRequest{
		AccessToken:           "the-token",
		CreateUserIfNotExists: false,
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.LoginReply{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgID:   "test",
			OrgName: "testOrg",
		},
	}
	mockClients.MockAuth.EXPECT().Login(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandlerEmbed)
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

	// Make sure no cookies are set.
	assert.Equal(t, 0, len(rr.Header().Values("Set-Cookie")))
}

func TestAuthLoginHandlerEmbed_WithAPIKey(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{}"))
	req.Header.Add("pixie-api-key", "test-key")
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: "test-key",
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.GetAugmentedTokenForAPIKeyResponse{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
	}
	mockClients.MockAuth.EXPECT().GetAugmentedTokenForAPIKey(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandlerEmbed)
	h.ServeHTTP(rr, req)

	assert.Equal(t, http.StatusOK, rr.Code)
	var parsedResponse struct {
		Token       string
		ExpiresAt   int64
		UserCreated bool `json:"userCreated"`
	}
	err = json.NewDecoder(rr.Body).Decode(&parsedResponse)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, parsedResponse.Token)
	assert.Equal(t, testTokenExpiry, parsedResponse.ExpiresAt)
	assert.Equal(t, false, parsedResponse.UserCreated)

	// Make sure no cookies are set.
	assert.Equal(t, 0, len(rr.Header().Values("Set-Cookie")))
}

func TestAuthLoginHandler_WithAPIKey(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("POST", "/login",
		strings.NewReader("{}"))
	req.Header.Add("pixie-api-key", "test-key")
	require.NoError(t, err)

	expectedAuthServiceReq := &authpb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: "test-key",
	}
	testReplyToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	testTokenExpiry := time.Now().Add(1 * time.Minute).Unix()
	loginResp := &authpb.GetAugmentedTokenForAPIKeyResponse{
		Token:     testReplyToken,
		ExpiresAt: testTokenExpiry,
	}
	mockClients.MockAuth.EXPECT().GetAugmentedTokenForAPIKey(gomock.Any(), expectedAuthServiceReq).Return(loginResp, nil)

	rr := httptest.NewRecorder()
	h := handler.New(env, controllers.AuthLoginHandler)
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

	// Check the token in the cookie.
	rawCookies := rr.Header().Get("Set-Cookie")
	header := http.Header{}
	header.Add("Cookie", rawCookies)
	req2 := http.Request{Header: header}
	sess, err := controllers.GetDefaultSession(env, &req2)
	require.NoError(t, err)
	assert.Equal(t, testReplyToken, sess.Values["_at"])
}
