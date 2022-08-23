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

package datastore

import (
	"context"
	"fmt"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

// ExperimentState represents the current state of an experiment (i.e. running, waiting for tasks, finished, errored, etc)
type ExperimentState string

const (
	// WaitingForTasks means the experiment is still waiting on a cluster or the build to complete.
	WaitingForTasks ExperimentState = "WAITING_TASKS"
	// Running means the experiment is being executed by the runner service.
	Running ExperimentState = "RUNNING"
	// Finished means the experiment completed successfully.
	Finished ExperimentState = "FINISHED"
	// ExperimentErrored means there was an error during some stage of the experiment and it has not yet been retried.
	ExperimentErrored ExperimentState = "ERROR"
	// RetryBackoff means the experiment has been retried the maximum number of times.
	RetryBackoff ExperimentState = "RETRY_BACKOFF"
)

// Datastore implementation for coordinator using a PGSQL backend.
type Datastore struct {
	db *sqlx.DB
}

// NewDatastore creates a new experiment datastore for implementing the controllers.ExperimentDatastore interface.
func NewDatastore(db *sqlx.DB) *Datastore {
	return &Datastore{
		db: db,
	}
}

// PushExperiment adds a new experiment to the experiment queue.
func (d *Datastore) PushExperiment(ctx context.Context, id uuid.UUID, spec *experimentpb.ExperimentSpec, tasksWaiting int) error {
	specBytes, err := spec.Marshal()
	if err != nil {
		return err
	}
	query := `INSERT INTO experiment_queue (id, serialized_spec, state, num_tasks_waiting, num_retries) VALUES ($1, $2, $3, $4, 0)`
	_, err = d.db.ExecContext(ctx, query, id, specBytes, string(WaitingForTasks), tasksWaiting)
	return err
}

// TaskComplete updates the experiment queue to reflect that a task for a given experiment has finished.
func (d *Datastore) TaskComplete(ctx context.Context, id uuid.UUID) (int, error) {
	// Set state to Running if this update would reduce num_tasks_waiting to 0.
	query := `UPDATE experiment_queue SET
    num_tasks_waiting = num_tasks_waiting - 1,
    state = COALESCE(
      (SELECT state FROM experiment_queue WHERE id = $1 AND num_tasks_waiting != 1),
      $2
    )
  WHERE id = $1
  RETURNING num_tasks_waiting
  `
	row := d.db.QueryRowxContext(ctx, query, id, Running)
	if row.Err() != nil {
		return 0, row.Err()
	}
	var remaining int
	if err := row.Scan(&remaining); err != nil {
		return 0, err
	}
	return remaining, nil
}

// GetExperimentSpec returns the ExperimentSpec proto for a given experiment.
func (d *Datastore) GetExperimentSpec(ctx context.Context, id uuid.UUID) (*experimentpb.ExperimentSpec, error) {
	query := `SELECT serialized_spec FROM experiment_queue WHERE id = $1`
	var serialized []byte
	err := d.db.GetContext(ctx, &serialized, query, id)
	if err != nil {
		return nil, err
	}
	spec := &experimentpb.ExperimentSpec{}
	err = spec.Unmarshal(serialized)
	if err != nil {
		return nil, err
	}
	return spec, nil
}

func (d *Datastore) updateExperimentState(ctx context.Context, id uuid.UUID, state ExperimentState) error {
	query := `UPDATE experiment_queue SET state = $2 WHERE id = $1`
	res, err := d.db.ExecContext(ctx, query, id, string(state))
	if err != nil {
		return err
	}
	rows, err := res.RowsAffected()
	if err != nil {
		return err
	}
	if rows == 0 {
		return fmt.Errorf("no experiments with ID: %s", id.String())
	}
	return nil
}

// ExperimentFinished sets the experiment state as FINISHED for the given experiment.
func (d *Datastore) ExperimentFinished(ctx context.Context, id uuid.UUID) error {
	return d.updateExperimentState(ctx, id, Finished)
}

// ExperimentFailed sets the experiment state for the given experiment as ERROR.
// TODO(james): store error here.
func (d *Datastore) ExperimentFailed(ctx context.Context, id uuid.UUID) error {
	query := `
  UPDATE experiment_queue SET
    state = $2
  WHERE id = $1
  `
	_, err := d.db.ExecContext(ctx, query, id, ExperimentErrored)
	if err != nil {
		return err
	}
	return nil
}

// RetryExperiment updates the experiment in the queue to reflect that it has been retried.
func (d *Datastore) RetryExperiment(ctx context.Context, id uuid.UUID, tasks int) error {
	query := `
  UPDATE experiment_queue SET
    state = $3,
    num_tasks_waiting = $2,
    num_retries = num_retries + 1
  WHERE id = $1
  `
	_, err := d.db.ExecContext(ctx, query, id, tasks, WaitingForTasks)
	if err != nil {
		return err
	}
	return nil
}

// BackoffExperiment updates the experiment state to RETRY_BACKOFF.
func (d *Datastore) BackoffExperiment(ctx context.Context, id uuid.UUID) error {
	return d.updateExperimentState(ctx, id, RetryBackoff)
}

// NumRetries returns the number of retries attempts for a given experiment.
func (d *Datastore) NumRetries(ctx context.Context, id uuid.UUID) (int64, error) {
	query := `SELECT num_retries FROM experiment_queue WHERE id = $1`
	row := d.db.QueryRowxContext(ctx, query, id)
	if row.Err() != nil {
		return 0, row.Err()
	}
	var retries int64
	if err := row.Scan(&retries); err != nil {
		return 0, err
	}
	return retries, nil
}
