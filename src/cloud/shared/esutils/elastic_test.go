package esutils_test

import (
	"os"
	"testing"

	"github.com/olivere/elastic/v7"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var elasticClient *elastic.Client

func TestMain(m *testing.M) {
	es, cleanup := testingutils.SetupElastic()
	elasticClient = es
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}
