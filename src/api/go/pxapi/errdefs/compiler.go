package errdefs

import (
	"fmt"

	publicvizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
)

// CompilerMultiError is an implementation of a multi-error for compiler messages.
type CompilerMultiError struct {
	errors []error
}

// Error returns the string representation of the error.
func (c CompilerMultiError) Error() string {
	s := fmt.Sprintf("Compilation failed: \n")
	for _, e := range c.errors {
		s += fmt.Sprintf("%s\n", e.Error())
	}
	return s
}

// Errors returns the list of underlying errors.
func (c CompilerMultiError) Errors() []error {
	return c.errors
}

// Unwrap makes this error wrap a generic compilation error.
func (c CompilerMultiError) Unwrap() error {
	return ErrCompilation

}

func newCompilerMultiError(errs ...error) error {
	e := CompilerMultiError{
		errors: make([]error, len(errs)),
	}
	for i, err := range errs {
		e.errors[i] = err
	}
	return e
}

// CompilerErrorDetails is an interface to access error details from the compiler.
type CompilerErrorDetails interface {
	Line() int64
	Column() int64
	Message() int64
}

type compilerErrorWithDetails struct {
	line    int64
	column  int64
	message string
}

func (e compilerErrorWithDetails) UnWrap() error {
	return ErrCompilation
}

func (e compilerErrorWithDetails) Error() string {
	return fmt.Sprintf("%d:%d %s", e.line, e.column, e.message)
}

func (e compilerErrorWithDetails) Line() int64 {
	return e.line
}

func (e compilerErrorWithDetails) Column() int64 {
	return e.line
}

func (e compilerErrorWithDetails) Message() string {
	return e.message
}

func newCompilerErrorWithDetails(e *publicvizierapipb.CompilerError) compilerErrorWithDetails {
	return compilerErrorWithDetails{
		line:    int64(e.Line),
		column:  int64(e.Column),
		message: e.Message,
	}
}
