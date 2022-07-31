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

package components

import (
	"fmt"

	"github.com/fatih/color"
	"github.com/spf13/viper"
	"github.com/vbauerster/mpb/v4"
	"github.com/vbauerster/mpb/v4/decor"
)

const barWidth = 4

// TaskInfo is the information associated with a task.
type TaskInfo struct {
	bar *mpb.Bar
	sd  *statusDecorator
	evd *errorViewDecorator
}

// Complete finishes the task.
func (t *TaskInfo) Complete(err error) {
	t.bar.SetTotal(1, true)
	t.evd.setError(err)
	t.sd.setError(err)
}

// SpinnerTable is view for a job run table with spinners.
type SpinnerTable struct {
	m     *mpb.Progress
	tasks []*TaskInfo
}

// NewSpinnerTable creates a new table with Spinners.
func NewSpinnerTable() *SpinnerTable {
	var opt mpb.ContainerOption
	if viper.GetBool("quiet") {
		opt = mpb.WithOutput(nil)
	}

	return &SpinnerTable{
		mpb.New(opt),
		make([]*TaskInfo, 0),
	}
}

// AddTask puts a task on the display.
func (s *SpinnerTable) AddTask(name string) *TaskInfo {
	ti := &TaskInfo{}
	sd := newStatusDecorator(barWidth)
	evd := newErrorViewDecorator()
	// We treat the spinner is either done/not-done, so we only need progress of 1 and 0, respectively.
	maxProgress := int64(1)
	bar := s.m.AddSpinner(maxProgress, mpb.SpinnerOnLeft,
		mpb.PrependDecorators(sd),
		mpb.BarWidth(barWidth),
		mpb.AppendDecorators(
			decor.Name(name, decor.WC{W: len(name) + 1, C: decor.DidentRight}),
			evd),
		mpb.BarClearOnComplete())

	ti.sd = sd
	ti.evd = evd
	ti.bar = bar
	s.tasks = append(s.tasks, ti)

	return ti
}

// Wait for all the spinners to complete.
func (s *SpinnerTable) Wait() {
	s.m.Wait()
}

type statusDecorator struct {
	decor.WC
	err   error
	width int
}

func newStatusDecorator(width int) *statusDecorator {
	wc := decor.WC{}
	wc.Init()
	return &statusDecorator{wc, nil, width}
}

// Decor is the output function for this decorator.
func (d *statusDecorator) Decor(stat *decor.Statistics) string {
	if !stat.Completed {
		return ""
	}
	if d.err != nil {
		return StatusErr(d.width)
	}
	return StatusOK(d.width)
}

func (d *statusDecorator) setError(err error) {
	d.err = err
}

type errorViewDecorator struct {
	decor.WC
	err error
}

func newErrorViewDecorator() *errorViewDecorator {
	wc := decor.WC{}
	wc.Init()
	return &errorViewDecorator{wc, nil}
}

// Decor is the output function for this decorator.
func (d *errorViewDecorator) Decor(stat *decor.Statistics) string {
	if !stat.Completed {
		return ""
	}
	if d.err == nil {
		return ""
	}
	return color.RedString(fmt.Sprintf(" ERR: %s", d.err.Error()))
}

func (d *errorViewDecorator) setError(err error) {
	d.err = err
}
