package utils

import (
	"bytes"
	"errors"
	"os/exec"
)

// ExecCommand runs a command and returns stderr as the error.
func ExecCommand(name string, args ...string) error {
	kcmd := exec.Command(name, args...)
	var stderr bytes.Buffer
	kcmd.Stderr = &stderr
	err := kcmd.Run()

	if err != nil {
		return errors.New(stderr.String())
	}
	return nil
}
