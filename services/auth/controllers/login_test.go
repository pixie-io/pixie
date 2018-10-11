package controllers_test

import (
	"bytes"
	"encoding/json"
	"errors"
	"io/ioutil"
	"net/http"
	"net/http/httptest"
	"net/url"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/services/auth/controllers"
	"pixielabs.ai/pixielabs/services/auth/controllers/mock"
)

func TestPostLogin(t *testing.T) {
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

	loginHandler := controllers.MakeHandleLoginFunc(a, "jwtkey")
	rr := doLoginRequest(t, loginHandler)

	// Check the response data.
	assert.Equal(t, http.StatusOK, rr.Code)
	body, err := ioutil.ReadAll(rr.Body)
	assert.Nil(t, err)

	var parsedResponse controllers.LoginResponse
	err = json.Unmarshal(body, &parsedResponse)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, parsedResponse.ExpiresAt > currentTime && parsedResponse.ExpiresAt < maxExpiryTime)

	verifyToken(t, parsedResponse.Token, fakeUserInfoSecondRequest.AppMetadata.PLUserID, parsedResponse.ExpiresAt, "jwtkey")
}

func TestPostLogin_BadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("", errors.New("bad token"))

	loginHandler := controllers.MakeHandleLoginFunc(a, "jwtkey")
	rr := doLoginRequest(t, loginHandler)

	// Check the response data.
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
	body, err := ioutil.ReadAll(rr.Body)
	assert.Nil(t, err)
	assert.Contains(t, string(body), "failed to get user")
}

func TestPostLogin_HasPLUserID(t *testing.T) {
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

	loginHandler := controllers.MakeHandleLoginFunc(a, "jwtkey")
	rr := doLoginRequest(t, loginHandler)

	// Check the response data.
	assert.Equal(t, http.StatusOK, rr.Code)
	body, err := ioutil.ReadAll(rr.Body)
	assert.Nil(t, err)

	var parsedResponse controllers.LoginResponse
	err = json.Unmarshal(body, &parsedResponse)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, parsedResponse.ExpiresAt > currentTime && parsedResponse.ExpiresAt < maxExpiryTime)

	verifyToken(t, parsedResponse.Token, "pluserid", parsedResponse.ExpiresAt, "jwtkey")
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

func doLoginRequest(t *testing.T, loginHandler http.HandlerFunc) *httptest.ResponseRecorder {
	data := url.Values{}
	data.Set("access_token", "tokenabc")
	// Create the request and serve.
	req, err := http.NewRequest("POST", "/login", bytes.NewBufferString(data.Encode()))
	assert.Nil(t, err)

	req.Header.Set("Content-Type", "application/x-www-form-urlencoded; param=value")
	rr := httptest.NewRecorder()
	loginHandler.ServeHTTP(rr, req)

	return rr
}
