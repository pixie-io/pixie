package healthz_test

import (
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/healthz"
)

func TestInstallPathHandler(t *testing.T) {
	mux := http.NewServeMux()
	healthz.InstallPathHandler(mux, "/healthz/test")
	req, err := http.NewRequest("GET", "http://abc.com/healthz/test", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))
}

func TestRegisterDefaultChecks(t *testing.T) {
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	req, err := http.NewRequest("GET", "http://abc.com/healthz", nil)
	require.NoError(t, err)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))

	req, err = http.NewRequest("GET", "http://abc.com/ping", nil)
	require.NoError(t, err)
	w = httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)
	assert.True(t, strings.HasPrefix(w.Body.String(), "OK"))
}

func TestMultipleChecks(t *testing.T) {
	var checks []healthz.Checker
	checks = append(checks, healthz.NamedCheck("bad-check", func() error {
		return fmt.Errorf("failed check")
	}))
	checks = append(checks, healthz.NamedCheck("good", func() error {
		return nil
	}))

	tests := []struct {
		path                       string
		expectedResponseStartsWith string
		expectedStatus             int
	}{
		{"/healthz", "FAILED", http.StatusInternalServerError},
		{"/healthz/bad-check", "FAILED", http.StatusInternalServerError},
		{"/healthz/good", "OK", http.StatusOK},
	}

	for _, test := range tests {
		mux := http.NewServeMux()
		healthz.RegisterDefaultChecks(mux, checks...)
		req, err := http.NewRequest("GET", fmt.Sprintf("http://abc.com%s", test.path), nil)
		require.NoError(t, err)

		w := httptest.NewRecorder()
		mux.ServeHTTP(w, req)
		assert.Equal(t, test.expectedStatus, w.Code)
		assert.True(t, strings.HasPrefix(w.Body.String(), test.expectedResponseStartsWith))
	}
}
