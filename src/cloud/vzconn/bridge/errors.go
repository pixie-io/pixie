package bridge

import "errors"

var (
	// ErrMissingRegistrationMessage is produced if registration was not the first message.
	ErrMissingRegistrationMessage = errors.New("Expected registration message")
	// ErrBadRegistrationMessage ia produced if a malformed registration message is received.
	ErrBadRegistrationMessage = errors.New("Malformed registration message")
	// ErrRegistrationFailedUnknown is the error for vizier registration failure.
	ErrRegistrationFailedUnknown = errors.New("registration failed unknown")
	// ErrRegistrationFailedNotFound is the error for vizier registration failure when vizier is not found.
	ErrRegistrationFailedNotFound = errors.New("registration failed not found")
	// ErrRequestChannelClosed is an error returned when the streams have already been closed.
	ErrRequestChannelClosed = errors.New("request channel already closed")
)
