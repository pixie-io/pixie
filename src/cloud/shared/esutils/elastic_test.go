package esutils_test

import (
	"log"
	"os"
	"testing"

	"github.com/olivere/elastic/v7"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var elasticClient *elastic.Client

func TestMain(m *testing.M) {
	es, cleanup, err := testingutils.SetupElastic()
	if err != nil {
		cleanup()
		log.Fatal(err)
	}
	elasticClient = es
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}
