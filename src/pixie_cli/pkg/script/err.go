package script

import "errors"

var (
	// ErrScriptNotFound is returned when the requested script does not exist.
	ErrScriptNotFound = errors.New("Script not found")
)
