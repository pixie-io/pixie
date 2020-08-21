package utils

import (
	"fmt"
	"strings"
)

func indent(i string) string {
	return "\t" + strings.Join(strings.Split(i, "\n"), "\n\t")
}

// ErrorAccumulator is the struct that stores accumulated errors.
type ErrorAccumulator struct {
	errorStrs []string
}

// AddError accumulates the passed in error if its not nil.
func (ea *ErrorAccumulator) AddError(e error) {
	if e == nil {
		return
	}
	ea.errorStrs = append(ea.errorStrs, e.Error())
}

// Merge returns a merged representation of the accumulated errors.
func (ea *ErrorAccumulator) Merge() error {
	if len(ea.errorStrs) == 0 {
		return nil
	}
	return fmt.Errorf(indent(strings.Join(ea.errorStrs, "\n")))
}

// MakeErrorAccumulator constructs the ErrorAccumulator.
func MakeErrorAccumulator() *ErrorAccumulator {
	return &ErrorAccumulator{
		errorStrs: make([]string, 0),
	}
}
