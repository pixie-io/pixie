/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controller_test

import (
	"context"
	"log"
	"os"
	"testing"

	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/jobs/elastic_migration/controller"
	"px.dev/pixie/src/cloud/jobs/elastic_migration/schema"
	"px.dev/pixie/src/utils/testingutils"
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
	require.NoError(t, err)

	response, err := elasticClient.IndexGet(indexName).Do(context.TODO())
	require.NoError(t, err)
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
	require.NoError(t, err)
	// On the second creation we do have an issue.
	err = im.PrepareIndex(indexName, string(indexMapping))
	assert.EqualError(t, err, "index 'test2' already exists, cannot prepare it")
}
