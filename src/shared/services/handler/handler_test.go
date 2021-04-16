package handler_test

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/handler"
)

func TestHandler_ServeHTTP(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return nil
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusOK, rw.Code)
}

func TestHandler_ServeHTTP_StatusError(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return &handler.StatusError{http.StatusUnauthorized, errors.New("badness")}
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusUnauthorized, rw.Code)
	assert.Equal(t, "badness\n", rw.Body.String())
}

func TestHandler_ServeHTTP_RegularError(t *testing.T) {
	testHandler := func(e env.Env, w http.ResponseWriter, r *http.Request) error {
		return errors.New("badness")
	}
	h := handler.New(env.New("test"), testHandler)

	req, err := http.NewRequest("GET", "http://test.com/", nil)
	require.NoError(t, err)

	rw := httptest.NewRecorder()
	h.ServeHTTP(rw, req)
	assert.Equal(t, http.StatusInternalServerError, rw.Code)
	assert.Equal(t, "Internal Server Error\n", rw.Body.String())
}
