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

package datastore_test

import (
	"context"
	"fmt"
	"os"
	"testing"

	"github.com/gofrs/uuid"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/e2e_test/perf_tool/backend/coordinator/datastore"
	"px.dev/pixie/src/e2e_test/perf_tool/backend/coordinator/schema"
	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/shared/services/pgtest"
)

var testID1 = uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000")
var testSpec1 = &experimentpb.ExperimentSpec{
	VizierSpec: &experimentpb.WorkloadSpec{},
}

func deleteDataFromDB(db *sqlx.DB) {
	db.MustExec(`DELETE from experiment_queue`)
}

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

var db *sqlx.DB

func testMain(m *testing.M) error {
	s := bindata.Resource(schema.AssetNames(), schema.Asset)
	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

type pushExperimentAction struct {
	id       uuid.UUID
	spec     *experimentpb.ExperimentSpec
	numTasks int
}

type taskCompleteAction struct {
	id                uuid.UUID
	expectedRemaining int
}

type getExperimentSpecAction struct {
	id           uuid.UUID
	expectedSpec *experimentpb.ExperimentSpec
}

type finishedAction struct {
	id uuid.UUID
}

type failedAction struct {
	id              uuid.UUID
	expectedRetries int
}

type retryAction struct {
	id       uuid.UUID
	numTasks int
}

type backoffAction struct {
	id uuid.UUID
}

type testAction struct {
	PushExperiment    *pushExperimentAction
	GetExperimentSpec *getExperimentSpecAction
	TaskComplete      *taskCompleteAction
	Finished          *finishedAction
	Failed            *failedAction
	Retry             *retryAction
	Backoff           *backoffAction
}

type dbExperiment struct {
	ID             uuid.UUID
	SerializedSpec []byte `db:"serialized_spec"`
	State          string
	TasksWaiting   int `db:"num_tasks_waiting"`
	Retries        int `db:"num_retries"`
}

func expectExperiment(t *testing.T, id uuid.UUID, spec *experimentpb.ExperimentSpec, state datastore.ExperimentState, tasksWaiting int, retries int) {
	e := &dbExperiment{}
	query := `SELECT id, serialized_spec, state, num_tasks_waiting, num_retries FROM experiment_queue WHERE id = $1`
	err := db.Get(e, query, id)
	require.NoError(t, err)
	assert.Equal(t, id, e.ID)
	if spec != nil {
		specFromDb := &experimentpb.ExperimentSpec{}
		err = specFromDb.Unmarshal(e.SerializedSpec)
		require.NoError(t, err)
		assert.Equal(t, spec, specFromDb)
	}
	assert.Equal(t, string(state), e.State)
	if tasksWaiting != -1 {
		assert.Equal(t, tasksWaiting, e.TasksWaiting)
	}
	if retries != -1 {
		assert.Equal(t, retries, e.Retries)
	}
}

func TestDatastore(t *testing.T) {
	tests := []struct {
		name    string
		actions []*testAction
	}{
		{
			"push",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:       testID1,
						spec:     testSpec1,
						numTasks: 2,
					},
				},
				{
					GetExperimentSpec: &getExperimentSpecAction{
						id:           testID1,
						expectedSpec: testSpec1,
					},
				},
			},
		},
		{
			"push and task complete",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:       testID1,
						spec:     testSpec1,
						numTasks: 2,
					},
				},
				{
					TaskComplete: &taskCompleteAction{
						id:                testID1,
						expectedRemaining: 1,
					},
				},
				{
					GetExperimentSpec: &getExperimentSpecAction{
						id:           testID1,
						expectedSpec: testSpec1,
					},
				},
			},
		},
		{
			"push and all tasks complete",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:       testID1,
						spec:     testSpec1,
						numTasks: 2,
					},
				},
				{
					TaskComplete: &taskCompleteAction{
						id:                testID1,
						expectedRemaining: 1,
					},
				},
				{
					TaskComplete: &taskCompleteAction{
						id:                testID1,
						expectedRemaining: 0,
					},
				},
				{
					GetExperimentSpec: &getExperimentSpecAction{
						id:           testID1,
						expectedSpec: testSpec1,
					},
				},
			},
		},
		{
			"experiment finished",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:   testID1,
						spec: testSpec1,
					},
				},
				{
					Finished: &finishedAction{
						id: testID1,
					},
				},
			},
		},
		{
			"experiment failed retry",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:   testID1,
						spec: testSpec1,
					},
				},
				{
					Failed: &failedAction{
						id:              testID1,
						expectedRetries: 0,
					},
				},
				{
					Retry: &retryAction{
						id:       testID1,
						numTasks: 2,
					},
				},
				{
					Failed: &failedAction{
						id:              testID1,
						expectedRetries: 1,
					},
				},
				{
					Retry: &retryAction{
						id:       testID1,
						numTasks: 2,
					},
				},
				{
					Finished: &finishedAction{
						id: testID1,
					},
				},
			},
		},
		{
			"backoff",
			[]*testAction{
				{
					PushExperiment: &pushExperimentAction{
						id:   testID1,
						spec: testSpec1,
					},
				},
				{
					Failed: &failedAction{
						id:              testID1,
						expectedRetries: 0,
					},
				},
				{
					Backoff: &backoffAction{
						id: testID1,
					},
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			deleteDataFromDB(db)
			d := datastore.NewDatastore(db)
			for _, a := range test.actions {
				if a.PushExperiment != nil {
					err := d.PushExperiment(context.Background(), a.PushExperiment.id, a.PushExperiment.spec, a.PushExperiment.numTasks)
					require.NoError(t, err)
					expectExperiment(t, a.PushExperiment.id, a.PushExperiment.spec, datastore.WaitingForTasks, a.PushExperiment.numTasks, 0)
				}
				if a.TaskComplete != nil {
					remaining, err := d.TaskComplete(context.Background(), a.TaskComplete.id)
					require.NoError(t, err)
					assert.Equal(t, a.TaskComplete.expectedRemaining, remaining)
					if remaining == 0 {
						expectExperiment(t, a.TaskComplete.id, nil, datastore.Running, remaining, -1)
					} else {
						expectExperiment(t, a.TaskComplete.id, nil, datastore.WaitingForTasks, remaining, -1)
					}
				}
				if a.GetExperimentSpec != nil {
					spec, err := d.GetExperimentSpec(context.Background(), a.GetExperimentSpec.id)
					require.NoError(t, err)
					assert.Equal(t, a.GetExperimentSpec.expectedSpec, spec)
				}
				if a.Finished != nil {
					err := d.ExperimentFinished(context.Background(), a.Finished.id)
					require.NoError(t, err)
					expectExperiment(t, a.Finished.id, nil, datastore.Finished, -1, -1)
				}
				if a.Failed != nil {
					err := d.ExperimentFailed(context.Background(), a.Failed.id)
					require.NoError(t, err)
					expectExperiment(t, a.Failed.id, nil, datastore.ExperimentErrored, -1, a.Failed.expectedRetries)
				}
				if a.Retry != nil {
					err := d.RetryExperiment(context.Background(), a.Retry.id, a.Retry.numTasks)
					require.NoError(t, err)
					expectExperiment(t, a.Retry.id, nil, datastore.WaitingForTasks, a.Retry.numTasks, -1)
				}
				if a.Backoff != nil {
					err := d.BackoffExperiment(context.Background(), a.Backoff.id)
					require.NoError(t, err)
					expectExperiment(t, a.Backoff.id, nil, datastore.RetryBackoff, -1, -1)
				}
			}
		})
	}
}
