package testingutils_test

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestClockNow(t *testing.T) {
	clock := testingutils.NewTestClock(time.Unix(0, 10))

	assert.Equal(t, clock.Now(), time.Unix(0, 10))
}

func TestClockAdvance(t *testing.T) {
	clock := testingutils.NewTestClock(time.Unix(0, 10))

	clock.Advance(10)

	assert.Equal(t, clock.Now(), time.Unix(0, 20))
}
