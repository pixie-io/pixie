package pebbledb

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestKeyUpperBound_Simple(t *testing.T) {
	in := "prefix"
	upperBound := keyUpperBound([]byte(in))
	assert.Equal(t, "prefiy", string(upperBound))
}

func TestKeyUpperBound_Empty(t *testing.T) {
	in := ""
	upperBound := keyUpperBound([]byte(in))
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_Nil(t *testing.T) {
	upperBound := keyUpperBound(nil)
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_AllMax(t *testing.T) {
	in := []byte{255, 255, 255, 255}
	upperBound := keyUpperBound([]byte(in))
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_LastMax(t *testing.T) {
	in := []byte{40, 41, 42, 255}
	upperBound := keyUpperBound([]byte(in))
	assert.Equal(t, []byte{40, 41, 43}, upperBound)
}
