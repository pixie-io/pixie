package testingutils

import (
	"time"
)

// MockContext provides a mock context for testing.
type MockContext struct{}

// Deadline returns the deadline for the mock context.
func (ctx MockContext) Deadline() (deadline time.Time, ok bool) {
	return deadline, ok
}

// Done returns the done channel for the mock context.
func (ctx MockContext) Done() <-chan struct{} {
	ch := make(chan struct{})
	return ch
}

// Err returns the error for the mock context.
func (ctx MockContext) Err() error {
	return nil
}

// Value returns the value for the mock context.
func (ctx MockContext) Value(key interface{}) interface{} {
	return nil
}
