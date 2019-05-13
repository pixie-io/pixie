package testingutils

import (
	"time"
)

// TestClock gets time according to a fake constant time.
type TestClock struct {
	fakeNow time.Time
}

// NewTestClock creates a TestClock with the given time.
func NewTestClock(now time.Time) *TestClock {
	testClock := &TestClock{fakeNow: now}
	return testClock
}

// Advance increases test clock's current time by the given duration.
func (tc *TestClock) Advance(duration time.Duration) {
	tc.fakeNow = tc.fakeNow.Add(duration)
}

// Now returns the fake constant time.
func (tc TestClock) Now() time.Time { return tc.fakeNow }
