package main_test

import (
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/stirling/utils"
	"strings"
	"testing"
)

func TestPreprocess(t *testing.T) {
	testFile := `#include "./test.h"
#include <linux/sched.h>`
	reader := strings.NewReader(testFile)
	headers := []string{"./test.h"}
	expandedFile := main.Preprocess(reader, headers)
	expected := `//-----------------------------------
#pragma once
#ifdef __linux__

struct pidruntime_val_t {
  uint64_t timestamp;
  uint64_t run_time;
  char name[16];
};
#endif

//-----------------------------------
#include <linux/sched.h>
`
	assert.Equal(t, expected, expandedFile)
}
