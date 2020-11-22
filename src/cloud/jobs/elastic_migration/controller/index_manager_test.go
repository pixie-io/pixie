package controller_test

import (
	"context"
	"log"
	"os"
	"testing"

	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/jobs/elastic_migration/controller"
	"pixielabs.ai/pixielabs/src/cloud/jobs/elastic_migration/schema"
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

func TestServer_PrepareOnNonexistantIndex(t *testing.T) {
	indexName := "test"
	indexMapping := schema.MustAsset("schema.json")

	im := controller.NewIndexManager(elasticClient)
	err := im.PrepareIndex(indexName, string(indexMapping))
	assert.Nil(t, err)

	response, err := elasticClient.IndexGet(indexName).Do(context.TODO())
	assert.Nil(t, err)
	// Check to make sure the index is actually there.
	_, ok := response[indexName]
	assert.True(t, ok)
}

func TestServer_PrepareOnExistingIndex(t *testing.T) {
	// Different name to not overlap with the above test.
	indexName := "test2"
	indexMapping := schema.MustAsset("schema.json")

	im := controller.NewIndexManager(elasticClient)
	// On the first creation, we don't have an issue.
	err := im.PrepareIndex(indexName, string(indexMapping))
	assert.Nil(t, err)
	// On the second creation we do have an issue.
	err = im.PrepareIndex(indexName, string(indexMapping))
	assert.EqualError(t, err, "index 'test2' already exists, cannot prepare it")
}
