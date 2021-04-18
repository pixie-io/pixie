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

package utils

import (
	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/pixie_cli/pkg/components"
)

// Task is an entity that can be run.
type Task interface {
	Name() string
	Run() error
}

// SerialTaskRunner runs tasks in serial and displays them in a table.
type SerialTaskRunner struct {
	tasks []Task
}

// NewSerialTaskRunner creates a new SerialTaskRunner
func NewSerialTaskRunner(tasks []Task) *SerialTaskRunner {
	return &SerialTaskRunner{
		tasks: tasks,
	}
}

// RunAndMonitor runs tasks and shows output in a table.
func (s *SerialTaskRunner) RunAndMonitor() error {
	st := components.NewSpinnerTable()
	defer st.Wait()
	for _, t := range s.tasks {
		ti := st.AddTask(t.Name())
		err := t.Run()
		ti.Complete(err)
		if err != nil {
			return err
		}
	}
	return nil
}

// ParallelTaskRunner runs tasks in parallel and displays them in a table.
type ParallelTaskRunner struct {
	tasks []Task
}

// NewParallelTaskRunner creates a new ParallelTaskRunner
func NewParallelTaskRunner(tasks []Task) *ParallelTaskRunner {
	return &ParallelTaskRunner{
		tasks: tasks,
	}
}

// RunAndMonitor runs tasks and shows output in a table.
func (s *ParallelTaskRunner) RunAndMonitor() error {
	st := components.NewSpinnerTable()
	g := errgroup.Group{}
	for _, t := range s.tasks {
		boundTask := t
		g.Go(func() error {
			ti := st.AddTask(boundTask.Name())
			err := boundTask.Run()
			ti.Complete(err)
			return err
		})
	}
	err := g.Wait()
	st.Wait()
	return err
}
