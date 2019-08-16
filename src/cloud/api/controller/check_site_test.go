package controller_test

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"reflect"
	"testing"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/golang/mock/gomock"
	mock_sitemanagerpb "pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb/mock"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"

	"github.com/spf13/viper"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
)

// TODO(zasgar): Refactor into services utils.
// JSONBytesEqual compares the JSON in two byte slices.
func JSONBytesEqual(t *testing.T, a, b []byte) {
	var j, j2 interface{}
	if err := json.Unmarshal(a, &j); err != nil {
		t.Fatalf("failed to unmarshal: %v", err)
	}
	if err := json.Unmarshal(b, &j2); err != nil {
		t.Fatalf("failed to unmarshal: %v", err)
	}
	assert.True(t, reflect.DeepEqual(j2, j))
}

func init() {
	// We need this to create env.
	viper.Set("session_key", "abcd")
}

func TestCheckSiteHandler_HandlerFunc(t *testing.T) {
	// We need this to create env.
	viper.Set("session_key", "abcd")

	ctrl := gomock.NewController(t)
	sc := mock_sitemanagerpb.NewMockSiteManagerServiceClient(ctrl)
	env, err := apienv.New(nil, sc)
	require.Nil(t, err)
	cs := controller.NewCheckSiteHandler(env)

	req, err := http.NewRequest("GET", "/check-site?domain_name=blah", nil)
	require.Nil(t, err)

	t.Run("site does not exists", func(t *testing.T) {
		sc.EXPECT().
			IsSiteAvailable(gomock.Any(), &sitemanagerpb.IsSiteAvailableRequest{DomainName: "blah"}).
			Return(&sitemanagerpb.IsSiteAvailableResponse{Available: true}, nil)

		rr := httptest.NewRecorder()
		cs.HandlerFunc(rr, req)

		assert.Equal(t, rr.Code, http.StatusOK)
		JSONBytesEqual(t, rr.Body.Bytes(), []byte("{\"available\":true}"))
	})

	t.Run("site already exists", func(t *testing.T) {
		sc.EXPECT().
			IsSiteAvailable(gomock.Any(), &sitemanagerpb.IsSiteAvailableRequest{DomainName: "blah"}).
			Return(&sitemanagerpb.IsSiteAvailableResponse{Available: false}, nil)

		rr := httptest.NewRecorder()
		cs.HandlerFunc(rr, req)

		assert.Equal(t, rr.Code, http.StatusOK)
		JSONBytesEqual(t, rr.Body.Bytes(), []byte("{\"available\":false}"))
	})

	t.Run("error on request", func(t *testing.T) {
		sc.EXPECT().
			IsSiteAvailable(gomock.Any(), &sitemanagerpb.IsSiteAvailableRequest{DomainName: "blah"}).
			Return(nil, status.Error(codes.Internal, "badness"))

		rr := httptest.NewRecorder()
		cs.HandlerFunc(rr, req)

		assert.Equal(t, rr.Code, http.StatusInternalServerError)
	})
}

func TestCheckSiteHandler_HandlerFunc_BadInput(t *testing.T) {
	// We need this to create env.
	viper.Set("session_key", "abcd")

	ctrl := gomock.NewController(t)
	sc := mock_sitemanagerpb.NewMockSiteManagerServiceClient(ctrl)
	env, err := apienv.New(nil, sc)
	require.Nil(t, err)
	cs := controller.NewCheckSiteHandler(env)

	t.Run("missing domain name", func(t *testing.T) {
		req, err := http.NewRequest("GET", "/check-site", nil)
		require.Nil(t, err)

		rr := httptest.NewRecorder()
		cs.HandlerFunc(rr, req)

		assert.Equal(t, rr.Code, http.StatusBadRequest)
	})

	t.Run("short domain name", func(t *testing.T) {
		req, err := http.NewRequest("GET", "/check-site?domain_name=t", nil)
		require.Nil(t, err)

		rr := httptest.NewRecorder()
		cs.HandlerFunc(rr, req)

		assert.Equal(t, rr.Code, http.StatusBadRequest)
	})
}
