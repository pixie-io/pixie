package cmd_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/cmd"
)

func TestVersionMajorNotCompatible(t *testing.T) {
	compatible, err := cmd.VersionCompatible("1.2", "2.1")
	assert.False(t, compatible)
	assert.Nil(t, err)
}

func TestVersionMinorNotCompatible(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.1", "2.2")
	assert.False(t, compatible)
	assert.Nil(t, err)
}

func TestVersionCompatibleSameMajor(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.4", "2.2")
	assert.True(t, compatible)
	assert.Nil(t, err)
}

func TestVersionCompatibleDifferentMajor(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.1", "1.4")
	assert.True(t, compatible)
	assert.Nil(t, err)
}

func TestVersionVersionWrongFormat(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.1.3", "1.4")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}

func TestVersionMinVersionWrongFormat(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.1", "1.4.4")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}

func TestVersionVersionMajorMalformed(t *testing.T) {
	compatible, err := cmd.VersionCompatible("a.1", "1.4")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}

func TestVersionVersioMinorMalformed(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.a", "1.4")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}

func TestVersionMinVersionMajorMalformed(t *testing.T) {
	compatible, err := cmd.VersionCompatible("1.1", "a.4")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}

func TestVersionMinVersioMinorMalformed(t *testing.T) {
	compatible, err := cmd.VersionCompatible("2.1", "1.a")
	assert.False(t, compatible)
	assert.NotNil(t, err)
}
