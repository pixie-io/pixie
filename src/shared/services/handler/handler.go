package handler

import (
	"errors"
	"net/http"

	"px.dev/pixie/src/shared/services/env"
)

// Error represents a handler error. It provides methods for a HTTP status
// code and embeds the built-in error interface.
type Error interface {
	error
	Status() int
}

// StatusError represents an error with an associated HTTP status code.
type StatusError struct {
	Code int
	Err  error
}

// Error returns the status errors underlying error.
func (se StatusError) Error() string {
	return se.Err.Error()
}

// Status returns the HTTP status code.
func (se StatusError) Status() int {
	return se.Code
}

// NewStatusError creates a new status error with a code and msg.
func NewStatusError(code int, msg string) *StatusError {
	return &StatusError{
		Code: code,
		Err:  errors.New(msg),
	}
}

// Handler struct that takes a configured BaseEnv and a function matching
// our signature.
type Handler struct {
	env env.Env
	H   func(e env.Env, w http.ResponseWriter, r *http.Request) error
}

// New returns a new App handler.
func New(e env.Env, f func(e env.Env, w http.ResponseWriter, r *http.Request) error) *Handler {
	return &Handler{e, f}
}

// ServeHTTP allows our Handler type to satisfy http.Handler.
func (h Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	err := h.H(h.env, w, r)
	if err != nil {
		switch e := err.(type) {
		case Error:
			http.Error(w, e.Error(), e.Status())
		default:
			// Any error types we don't specifically look out for default
			// to serving a HTTP 500
			http.Error(w, http.StatusText(http.StatusInternalServerError),
				http.StatusInternalServerError)
		}
	}
}
