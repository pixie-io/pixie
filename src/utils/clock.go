package utils

import (
	"time"
)

// Clock is an interface that should be used when any time operations are needed.
type Clock interface {
	Now() time.Time
}

// SystemClock gets time according to the current system clock.
type SystemClock struct{}

// Now returns the current time on the system clock.
func (sc SystemClock) Now() time.Time { return time.Now() }
